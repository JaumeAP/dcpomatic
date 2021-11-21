/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "barrier.h"
#include "dcp_video.h"
#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "image.h"
#include "j2k_encoder_fastvideo_backend.h"
#include "player_video.h"
#include <dcp/rgb_xyz.h>
#include <fastvideo_encoder_j2k.h>
#include <fastvideo_sdk.h>
#include <common/EnumToStringSdk.h>


using std::vector;
using boost::optional;
using dcp::ArrayData;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif



J2KEncoderFastvideoBackend::J2KEncoderFastvideoBackend (Barrier& barrier)
	: _barrier (barrier)
{
	fastDeviceProperty* device_property;
	int devices;
	auto r = fastGetDevices(&device_property, &devices);
	if (r != FAST_OK) {
		throw FastvideoError ("GetDevices", r);
	}

	fastSdkParametersHandle_t sdk_parameters;
	r = fastGetSdkParametersHandle(&sdk_parameters);
	if (r != FAST_OK) {
		throw FastvideoError ("GetSdkParametersHandle", r);
	}
	r = fastEncoderJ2kLibraryInit(sdk_parameters);
	if (r != FAST_OK) {
		throw FastvideoError ("DecoderJ2kLibraryInit", r);
	}
	fastTraceCreate("/home/carl/trace.log");
}


J2KEncoderFastvideoBackend::~J2KEncoderFastvideoBackend ()
{
	if (_setup_done) {
		fastFree (_xyz_buffer);
		fastEncoderJ2kDestroy (_encoder);
		fastImportFromHostDestroy (_adapter);
	}
}


void
J2KEncoderFastvideoBackend::setup (dcp::Size size)
{
	auto r = fastImportFromHostCreate(
		&_adapter, FAST_RGB12, size.width, size.height, &_src_buffer
		);
	if (r != FAST_OK) {
		throw FastvideoError ("ImportFromHostCreate", r);
	}

	fastEncoderJ2kStaticParameters_t parameters;
	parameters.lossless = false;
	parameters.pcrdEnabled = true;
	parameters.dwtLevels = 6;
	parameters.codeblockSize = 32;
	parameters.maxQuality = 1.5;
	parameters.compressionRatio = 1;
	parameters.info = false;
	parameters.tier2Threads = 4;
	parameters.tileWidth = size.width;
	parameters.tileHeight = size.height;
	parameters.noMCT = false;
	parameters.ss1_x = 1;
	parameters.ss1_y = 1;
	parameters.ss2_x = 1;
	parameters.ss2_y = 1;
	parameters.ss3_x = 1;
	parameters.ss3_y = 1;
	parameters.yuvSubsampledFormat = false;

        r = fastEncoderJ2kCreate(
                &_encoder, &parameters, FAST_RGB12, size.width, size.height, quantity(), _src_buffer
		);
	if (r != FAST_OK) {
		throw FastvideoError ("EncoderJ2kCreate", r);
	}

        bool success = false;

        r = fastEncoderJ2kIsInitialized(_encoder, &success);
	if (r != FAST_OK || !success) {
		throw FastvideoError ("EncoderJ2kIsInitialized", r);
	}

	_xyz_buffer_stride = size.width * 6;
	_xyz_buffer_stride += 4 - (_xyz_buffer_stride % 4);
	r = fastMalloc(reinterpret_cast<void**>(&_xyz_buffer), _xyz_buffer_stride * size.height);
	if (r != FAST_OK) {
		throw FastvideoError ("Malloc", r);
	}
}


vector<ArrayData>
J2KEncoderFastvideoBackend::encode (vector<DCPVideo> const& video)
{
	DCPOMATIC_ASSERT (static_cast<int>(video.size()) <= quantity());
	_barrier.wait();

	if (!_setup_done) {
		setup (video.front().frame()->out_size());
		_setup_done = true;
	}

	for (auto const& i: video) {
		auto image = i.frame()->image(boost::bind(&PlayerVideo::keep_xyz_or_rgb, _1), VideoRange::FULL, false);
		if (i.frame()->colour_conversion()) {
			dcp::rgb_to_xyz (
				image->data()[0],
				image->size(),
				image->stride()[0],
				_xyz_buffer,
				_xyz_buffer_stride,
				i.frame()->colour_conversion().get()
				);
		} else {
			/* XXX */
		}

		auto r = fastImportFromHostCopy(
			_adapter,
			_xyz_buffer,
			image->size().width,
			_xyz_buffer_stride,
			image->size().height
			);
		if (r != FAST_OK) {
			throw FastvideoError ("ImportFromHostCopy", r);
		}

		fastEncoderJ2kDynamicParameters_t dynamic_parameters;
		dynamic_parameters.targetStreamSize = i.j2k_bandwidth() / (8 * i.frames_per_second());
		dynamic_parameters.quality = 1.0;
		dynamic_parameters.writeHeader = false;

		r = fastEncoderJ2kAddImageToBatch(
			_encoder,
			&dynamic_parameters,
			image->size().width,
			image->size().height
		);
		if (r != FAST_OK) {
			throw FastvideoError ("EncoderJ2kAddImageToBatch", r);
		}
	}


	fastEncoderJ2kReport_t report;
	fastEncoderJ2kOutput_t output;
	int constexpr max_j2k_size = 1024 * 1024 * 2;
	output.bufferSize = max_j2k_size;
	dcp::ArrayData data(output.bufferSize);
	output.byteStream = data.data();

	auto r = fastEncoderJ2kTransformBatch(_encoder, &output, &report);
	if (r != FAST_OK) {
		fastTraceClose();
		throw FastvideoError ("EncoderJ2KTransformBatch", r);
	}

	vector<dcp::ArrayData> encoded;
	for (size_t i = 0; i < video.size(); ++i) {
		data.set_size (output.streamSize);
		encoded.push_back (data);
		data = dcp::ArrayData(output.bufferSize);
		output.byteStream = data.data();
		int images_left = 0;
		r = fastEncoderJ2kGetNextEncodedImage (_encoder, &output, &report, &images_left);
	}

	return encoded;
}



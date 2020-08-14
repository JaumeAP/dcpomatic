#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "exceptions.h"
#include "image.h"
#include "j2k_encoder_fastvideo_backend.h"
#include "player_video.h"
#include <dcp/rgb_xyz.h>
#include <fastvideo_encoder_j2k.h>
#include <fastvideo_sdk.h>


using std::vector;
using boost::optional;
using boost::shared_ptr;
using dcp::Data;



J2KEncoderFastvideoBackend::J2KEncoderFastvideoBackend ()
	: _setup_done (false)
{
	fastSdkParametersHandle_t sdk_parameters;
	fastStatus_t r = fastGetSdkParametersHandle(&sdk_parameters);
	if (r != FAST_OK) {
		throw FastvideoError ("GetSdkParametersHandle", r);
	}
	r = fastEncoderJ2kLibraryInit(sdk_parameters);
	if (r != FAST_OK) {
		throw FastvideoError ("DecoderJ2kLibraryInit", r);
	}
}


J2KEncoderFastvideoBackend::~J2KEncoderFastvideoBackend ()
{
	if (_setup_done) {
		delete[] _xyz_buffer;
		fastEncoderJ2kDestroy (_encoder);
		fastImportFromHostDestroy (_adapter);
	}
}


void
J2KEncoderFastvideoBackend::setup (dcp::Size size)
{
	fastStatus_t r = fastImportFromHostCreate(
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
	parameters.maxQuality = 50;
	parameters.compressionRatio = 0.5;
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
	_xyz_buffer = new uint16_t[_xyz_buffer_stride * size.height];
}


vector<Data>
J2KEncoderFastvideoBackend::encode (vector<shared_ptr<DCPVideo> > video)
{
	DCPOMATIC_ASSERT (static_cast<int>(video.size()) == quantity());
	std::cout << "FV: " << video.size() << " from " << video.front()->index() << "\n";

	if (!_setup_done) {
		setup (video.front()->frame()->inter_size());
		_setup_done = true;
	}

	BOOST_FOREACH (shared_ptr<DCPVideo> i, video) {
		shared_ptr<dcpomatic::Image> image = i->frame()->image(boost::bind(&PlayerVideo::keep_xyz_or_rgb, _1), true, false);
		if (i->frame()->colour_conversion()) {
			dcp::rgb_to_xyz (
				image->data()[0],
				image->size(),
				image->stride()[0],
				_xyz_buffer,
				_xyz_buffer_stride,
				i->frame()->colour_conversion().get()
				);
		} else {
			/* XXX */
		}


		fastStatus_t r = fastImportFromHostCopy(
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
		dynamic_parameters.targetStreamSize = 0;
		dynamic_parameters.quality = 0.5;
		dynamic_parameters.writeHeader = false;

		r = fastEncoderJ2kAddImageToBatch(
			_encoder,
			&dynamic_parameters,
			image->size().width,
			image->size().height
		);
	}


	int free_slots = 0;
	fastEncoderJ2kFreeSlotsInBatch(_encoder, &free_slots);
	DCPOMATIC_ASSERT (free_slots == 0);

	fastEncoderJ2kReport_t report;
	fastEncoderJ2kOutput_t output;
	fastStatus_t r = fastEncoderJ2kTransformBatch(_encoder, &output, &report);
	if (r != FAST_OK) {
		throw FastvideoError ("EncoderJ2KTransformBatch", r);
	}

	vector<Data> encoded;
	for (size_t i = 0; i < video.size(); ++i) {
		encoded.push_back (Data(output.byteStream, output.streamSize));
		int images_left = 0;
		r = fastEncoderJ2kGetNextEncodedImage (_encoder, &output, &report, &images_left);
	}

	std::cout << "FV returns " << encoded.size() << "\n";
	return encoded;
}



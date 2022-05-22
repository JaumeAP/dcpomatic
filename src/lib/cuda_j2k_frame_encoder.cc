/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "cross.h"
#include "cuda_j2k_frame_encoder.h"
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "exceptions.h"
#include "player_video.h"
#include <dcp/openjpeg_image.h>
#include <nvjpeg2k.h>
#include <vector>


using std::vector;
using boost::optional;


CUDAJ2KFrameEncoder::CUDAJ2KFrameEncoder()
{
	nvjpeg2kEncoderCreateSimple(&_encoder_handle);
	nvjpeg2kEncodeStateCreate(_encoder_handle, &_encoder_state);
	nvjpeg2kEncodeParamsCreate(&_encoder_params);
}


CUDAJ2KFrameEncoder::Frame::Frame(DCPVideo const& vf)
{
	auto xyz = convert_to_xyz(vf.frame(), boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2));

	void* pixel_data_h[] = {
		xyz->data(0),
		xyz->data(1),
		xyz->data(2)
	};

	auto const pitch = xyz->size().width * 2;

	for (int i = 0; i < 3; ++i) {
		_pitch_in_bytes[i] = pitch;
		auto status = cudaMallocPitch(
			reinterpret_cast<void**>(&_pixel_data_d[i]),
			&_pitch_in_bytes[i],
			pitch,
			xyz->size().height
			);

		if (status != cudaSuccess) {
			throw CUDAError("cudaMallocPitch", status);
		}

		status = cudaMemcpy2D(
			_pixel_data_d[i],
			_pitch_in_bytes[i],
			pixel_data_h[i],
			_pitch_in_bytes[i],
			pitch,
			xyz->size().height,
			cudaMemcpyHostToDevice
			);

		if (status != cudaSuccess) {
			throw CUDAError("cudaMemcpy2D", status);
		}

		cudaDeviceSynchronize();
	}

	_device_image.num_components = 3;
	_device_image.pixel_data = reinterpret_cast<void**>(_pixel_data_d);
	_device_image.pixel_type = NVJPEG2K_UINT16;
	_device_image.pitch_in_bytes = reinterpret_cast<size_t*>(_pitch_in_bytes);
}


CUDAJ2KFrameEncoder::Frame::Frame(Frame&& other)
{
	for (int i = 0; i < 3; ++i) {
		_pixel_data_d[i] = other._pixel_data_d[i];
		other._pixel_data_d[i] = nullptr;
		_pitch_in_bytes[i] = other._pitch_in_bytes[i];
	}

	_device_image.num_components = other._device_image.num_components;
	_device_image.pixel_data = reinterpret_cast<void**>(_pixel_data_d);
	_device_image.pixel_type = NVJPEG2K_UINT16;
	_device_image.pitch_in_bytes = reinterpret_cast<size_t*>(_pitch_in_bytes);
}


CUDAJ2KFrameEncoder::Frame::~Frame()
{
	cudaFree(_pixel_data_d[0]);
	cudaFree(_pixel_data_d[1]);
	cudaFree(_pixel_data_d[2]);
}


vector<dcp::ArrayData>
CUDAJ2KFrameEncoder::encode(DCPVideo const& vf)
{
	int constexpr BATCH_SIZE = 128;

	auto const size = vf.frame()->out_size();
	DCPOMATIC_ASSERT(!_size || size == *_size);
	_size = size;

	DCPOMATIC_ASSERT(!_resolution || vf.resolution() == *_resolution);
	_resolution = vf.resolution();

	if (_batch.size() < BATCH_SIZE) {
		_batch.push_back(Frame(vf));
		return {};
	}

	return flush();
}


vector<dcp::ArrayData>
CUDAJ2KFrameEncoder::flush()
{
	if (_batch.empty()) {
		return {};
	}

	nvjpeg2kImageComponentInfo_t info[3];
	for (int i = 0; i < 3; ++i) {
		info[i].component_width = _size->width;
		info[i].component_height = _size->height;
		info[i].precision = 12;
		info[i].sgn = 0;
	}

	nvjpeg2kEncodeConfig_t config;
	memset(&config, 0, sizeof(config));
	config.stream_type = NVJPEG2K_STREAM_J2K;
	config.color_space = NVJPEG2K_COLORSPACE_SRGB;
	config.image_width = _size->width;
	config.image_height = _size->height;
	config.num_components = 3;
	config.image_comp_info = reinterpret_cast<nvjpeg2kImageComponentInfo_t*>(&info);
	config.code_block_w = 32;
	config.code_block_h = 32;
	config.irreversible = 0;
	config.mct_mode = 1;
	config.prog_order = NVJPEG2K_CPRL;
	config.num_resolutions = *_resolution == Resolution::FOUR_K ? 7 : 6;

	auto status = nvjpeg2kEncodeParamsSetEncodeConfig(_encoder_params, &config);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeParamsSetEncodeConfig", status);
	}

	// XXX: quality
	status = nvjpeg2kEncodeParamsSetQuality(_encoder_params, 25);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeParamsSetQuality", status);
	}

	vector<dcp::ArrayData> output;

	for (auto const& frame: _batch) {

		auto x = frame.device_image();

		status = nvjpeg2kEncode(_encoder_handle, _encoder_state, _encoder_params, frame.device_image(), 0);
		if (status != NVJPEG2K_STATUS_SUCCESS) {
			throw CUDAError("nvjpeg2kEncode", status);
		}

		size_t compressed_size;
		status = nvjpeg2kEncodeRetrieveBitstream(_encoder_handle, _encoder_state, nullptr, &compressed_size, 0);

		dcp::ArrayData this_output(compressed_size);
		status = nvjpeg2kEncodeRetrieveBitstream(_encoder_handle, _encoder_state, this_output.data(), &compressed_size, 0);
		if (status != NVJPEG2K_STATUS_SUCCESS) {
			throw CUDAError("nvjpeg2kEncodeRetrieveBitstream", status);
		}

		output.push_back(this_output);
		cudaStreamSynchronize(0);
	}

	_batch.clear();

	return output;
}


void
CUDAJ2KFrameEncoder::log_thread_start ()
{
       LOG_TIMING("start-encoder-thread thread=%1", thread_id());
}

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


using boost::optional;


CUDAJ2KFrameEncoder::CUDAJ2KFrameEncoder()
{
	nvjpeg2kEncoderCreateSimple(&_encoder_handle);
	nvjpeg2kEncodeStateCreate(_encoder_handle, &_encoder_state);
	nvjpeg2kEncodeParamsCreate(&_encoder_params);

	_pixel_data_d[0] = _pixel_data_d[1] = _pixel_data_d[2] = nullptr;
}


void
CUDAJ2KFrameEncoder::allocate(dcp::Size size)
{
	for (int i = 0; i < 3; ++i) {
		_image_component_info[i].component_width = size.width;
		_image_component_info[i].component_height = size.height;
		_image_component_info[i].precision = 12;
		_image_component_info[i].sgn = 0;
		_pitch_in_bytes[i] = size.width * 2;
	}

	for (int i = 0; i < 3; ++i) {
		auto error = cudaMallocPitch(
			reinterpret_cast<void**>(&_pixel_data_d[i]),
			&_pitch_in_bytes[i],
			_image_component_info[i].component_width * 2,
			_image_component_info[i].component_height
			);
		if (error != cudaSuccess) {
			throw CUDAError("cudaMallocPitch", error);
		}
	}

	_allocated_size = size;
}


CUDAJ2KFrameEncoder::~CUDAJ2KFrameEncoder()
{
	free();
}


void
CUDAJ2KFrameEncoder::free()
{
	for (int i = 0; i < 3; ++i) {
		cudaFree(&_pixel_data_d[i]);
	}

	_pixel_data_d[0] = _pixel_data_d[1] = _pixel_data_d[2] = nullptr;
}


optional<dcp::ArrayData>
CUDAJ2KFrameEncoder::encode(DCPVideo const& vf)
{
	std::cout << "cuda encode starts.\n";

	auto frame = vf.frame();
	auto size = frame->out_size();

	if (size != _allocated_size) {
		free();
		allocate(size);
	}

	auto xyz = convert_to_xyz(vf.frame(), boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2));

	for (int i = 0; i < 3; ++i) {
		_pixel_data_h[i] = reinterpret_cast<uint8_t*>(xyz->data(i));
	}

	nvjpeg2kImage_t image_h;
	image_h.num_components = 3;
	image_h.pixel_data = reinterpret_cast<void**>(_pixel_data_h);
	image_h.pixel_type = NVJPEG2K_UINT16;
	image_h.pitch_in_bytes = reinterpret_cast<size_t*>(_pitch_in_bytes);

	nvjpeg2kImage_t image_d;
	image_d.num_components = 3;
	image_d.pixel_data = reinterpret_cast<void**>(_pixel_data_d);
	image_d.pixel_type = NVJPEG2K_UINT16;
	image_d.pitch_in_bytes = reinterpret_cast<size_t*>(_pitch_in_bytes);

	for (int i = 0; i < 3; ++i) {
		auto status = cudaMemcpy2D(
			image_d.pixel_data[i],
			image_d.pitch_in_bytes[i],
			image_h.pixel_data[i],
			image_h.pitch_in_bytes[i],
			2 * _image_component_info[i].component_width,
			_image_component_info[i].component_height,
			cudaMemcpyHostToDevice
			);
		if (status != cudaSuccess) {
			throw CUDAError("cudaMemcpy2D", status);
		}
		cudaDeviceSynchronize();
	}

	nvjpeg2kEncodeConfig_t config;
	memset(&config, 0, sizeof(config));
	config.stream_type = NVJPEG2K_STREAM_J2K;
	config.color_space = NVJPEG2K_COLORSPACE_SRGB;
	config.image_width = size.width;
	config.image_height = size.height;
	config.num_components = 3;
	config.image_comp_info = reinterpret_cast<nvjpeg2kImageComponentInfo_t*>(&_image_component_info);
	config.code_block_w = 32;
	config.code_block_h = 32;
	config.irreversible = 0;
	config.mct_mode = 1;
	config.prog_order = NVJPEG2K_CPRL;
	config.num_resolutions = vf.resolution() == Resolution::FOUR_K ? 7 : 6;

	auto status = nvjpeg2kEncodeParamsSetEncodeConfig(_encoder_params, &config);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeParamsSetEncodeConfig", status);
	}

	// XXX: quality
	status = nvjpeg2kEncodeParamsSetQuality(_encoder_params, 25);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeParamsSetQuality", status);
	}

	status = nvjpeg2kEncode(_encoder_handle, _encoder_state, _encoder_params, &image_d, 0);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncode", status);
	}

	size_t compressed_size;
	status = nvjpeg2kEncodeRetrieveBitstream(_encoder_handle, _encoder_state, nullptr, &compressed_size, 0);

	dcp::ArrayData output(compressed_size);
	status = nvjpeg2kEncodeRetrieveBitstream(_encoder_handle, _encoder_state, output.data(), &compressed_size, 0);
	cudaDeviceSynchronize();

	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeRetrieveBitstream", status);
	}

	std::cout << "cuda encode ends with " << status << " " << output.size() << "\n";

	return output;
}


void
CUDAJ2KFrameEncoder::log_thread_start ()
{
       LOG_TIMING("start-encoder-thread thread=%1", thread_id());
}

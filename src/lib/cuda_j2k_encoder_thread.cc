/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "colour_conversion.h"
#include "cuda_j2k_encoder_thread.h"
#include "dcp_video.h"
#include "exceptions.h"
#include "resolution.h"
#include <nvjpeg2k.h>


using std::shared_ptr;


CUDAJ2KEncoderThread::CUDAJ2KEncoderThread(J2KEncoder& encoder)
	: J2KSyncEncoderThread(encoder)
{
	nvjpeg2kEncoderCreateSimple(&_encoder_handle);
	nvjpeg2kEncodeStateCreate(_encoder_handle, &_encode_state);
	nvjpeg2kEncodeParamsCreate(&_encode_params);

	nvjpeg2kEncodeParamsSetInputFormat(_encode_params, NVJPEG2K_FORMAT_PLANAR);

	cudaStreamCreateWithFlags(&_stream, cudaStreamNonBlocking);
}


shared_ptr<dcp::ArrayData>
CUDAJ2KEncoderThread::encode(DCPVideo const& frame)
{
	auto xyz = convert_to_xyz(frame.frame());
	auto const size = xyz->size();

	/* {x,y,z}lp is 4-byte int representation */
	int* xlp = xyz->data(0);
	int* ylp = xyz->data(1);
	int* zlp = xyz->data(2);

	uint8_t* pixel_data_h[3];

	/* {x,y,z}p is 2-byte int representation */
	_xyz_x.resize(size.width * size.height);
	int16_t* xp = _xyz_x.data();
	pixel_data_h[0] = reinterpret_cast<uint8_t*>(xp);

	_xyz_y.resize(size.width * size.height);
	int16_t* yp = _xyz_y.data();
	pixel_data_h[1] = reinterpret_cast<uint8_t*>(yp);

	_xyz_z.resize(size.width * size.height);
	int16_t* zp = _xyz_z.data();
	pixel_data_h[2] = reinterpret_cast<uint8_t*>(zp);

	for (int j = 0; j < size.width * size.height; ++j) {
		*xp++ = static_cast<int16_t>(*xlp++);
		*yp++ = static_cast<int16_t>(*ylp++);
		*zp++ = static_cast<int16_t>(*zlp++);
	}

	allocate_pixel_data_d(size);

	/* Copy to device */
	size_t pitch_in_bytes_h[3];
	for (int i = 0; i < 3; ++i) {
		pitch_in_bytes_h[i] = size.width * 2;
		auto const status = cudaMemcpy2D(
			_pixel_data_d[i],
			_pitch_in_bytes_d[i],
			pixel_data_h[i],
			pitch_in_bytes_h[i],
			size.width * 2,
			size.height,
			cudaMemcpyHostToDevice
			);

		if (status != cudaSuccess) {
			throw CUDAError("cudaMemcpy2D", status);
		}
	}

	nvjpeg2kImage_t device_image;
	device_image.num_components = 3;
	device_image.pixel_data = reinterpret_cast<void**>(_pixel_data_d);
	device_image.pixel_type = NVJPEG2K_INT16;
	device_image.pitch_in_bytes = reinterpret_cast<size_t*>(_pitch_in_bytes_d);

	nvjpeg2kImageComponentInfo_t info[3];
	for (int i = 0; i < 3; ++i) {
		info[i].component_width = size.width;
		info[i].component_height = size.height;
		info[i].precision = 12;
		info[i].sgn = 0;
	}

	nvjpeg2kEncodeConfig_t config;
	memset(&config, 0, sizeof(config));
	config.stream_type = NVJPEG2K_STREAM_J2K;
	config.color_space = NVJPEG2K_COLORSPACE_SRGB;
	config.image_width = size.width;
	config.image_height = size.height;
	config.num_components = 3;
	config.image_comp_info = reinterpret_cast<nvjpeg2kImageComponentInfo_t*>(&info);
	config.code_block_w = 32;
	config.code_block_h = 32;
	config.irreversible = 0;
	config.mct_mode = 1;
	config.prog_order = NVJPEG2K_CPRL;
	config.num_resolutions = frame.resolution() == Resolution::FOUR_K ? 7 : 6;
	config.num_precincts_init = 7;
	config.precinct_width[0] = 7;
	config.precinct_height[0] = 7;
	for (int i = 1; i < 6; ++i) {
		config.precinct_width[i] = 8;
		config.precinct_height[i] = 8;
	}

	auto status = nvjpeg2kEncodeParamsSetEncodeConfig(_encode_params, &config);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeParamsSetEncodeConfig", status);
	}

	// XXX: quality
	status = nvjpeg2kEncodeParamsSetQuality(_encode_params, 50);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeParamsSetQuality", status);
	}

	status = nvjpeg2kEncode(_encoder_handle, _encode_state, _encode_params, &device_image, _stream);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncode", status);
	}

	size_t compressed_size;
	status = nvjpeg2kEncodeRetrieveBitstream(_encoder_handle, _encode_state, nullptr, &compressed_size, _stream);

	auto output = std::make_shared<dcp::ArrayData>(compressed_size);
	status = nvjpeg2kEncodeRetrieveBitstream(_encoder_handle, _encode_state, output->data(), &compressed_size, _stream);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kEncodeRetrieveBitstream", status);
	}

	return output;
}


void
CUDAJ2KEncoderThread::allocate_pixel_data_d(dcp::Size size)
{
	if (size == _pixel_data_d_size) {
		return;
	}

	free_pixel_data_d();

	for (int i = 0; i < 3; ++i) {
		auto status = cudaMallocPitch(
			reinterpret_cast<void**>(&_pixel_data_d[i]),
			&_pitch_in_bytes_d[i],
			size.width * 2,
			size.height
			);

		if (status != cudaSuccess) {
			throw CUDAError("cudaMallocPitch", status);
		}
	}

	_pixel_data_d_size = size;
}


void
CUDAJ2KEncoderThread::free_pixel_data_d()
{
	for (int i = 0; i < 3; ++i) {
		cudaFree(_pixel_data_d[i]);
	}
}


CUDAJ2KEncoderThread::~CUDAJ2KEncoderThread()
{
	free_pixel_data_d();

	nvjpeg2kEncodeParamsDestroy(_encode_params);
	nvjpeg2kEncodeStateDestroy(_encode_state);
	nvjpeg2kEncoderDestroy(_encoder_handle);
}


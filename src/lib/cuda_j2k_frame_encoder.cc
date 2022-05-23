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
#include <dcp/array_data.h>
#include <dcp/openjpeg_image.h>
#include <nvjpeg2k.h>
#include <thread>
#include <vector>


using std::make_pair;
using std::thread;
using std::vector;
using boost::optional;


vector<thread> CUDAJ2KFrameEncoder::_cuda_threads;
std::queue<CUDAJ2KFrameEncoder::Input> CUDAJ2KFrameEncoder::_input;
std::map<std::pair<int, Eyes>, dcp::ArrayData> CUDAJ2KFrameEncoder::_output;
boost::condition CUDAJ2KFrameEncoder::_input_condition;
boost::mutex CUDAJ2KFrameEncoder::_input_mutex;
boost::condition CUDAJ2KFrameEncoder::_output_condition;
boost::mutex CUDAJ2KFrameEncoder::_output_mutex;


CUDAJ2KFrameEncoder::CUDAJ2KFrameEncoder()
{
	if (_cuda_threads.empty()) {
		for (int i = 0; i < 8; ++i) {
			_cuda_threads.push_back(std::thread(&CUDAJ2KFrameEncoder::cuda_thread));
		}
	}
}


void
CUDAJ2KFrameEncoder::cuda_thread()
{
	nvjpeg2kEncoder_t encoder_handle;
	nvjpeg2kEncodeState_t encoder_state;
	nvjpeg2kEncodeParams_t encoder_params;

	nvjpeg2kEncoderCreateSimple(&encoder_handle);
	nvjpeg2kEncodeStateCreate(encoder_handle, &encoder_state);
	nvjpeg2kEncodeParamsCreate(&encoder_params);

	cudaStream_t stream;
	cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking);

	while (true) {
		boost::mutex::scoped_lock lm(_input_mutex);
		while (_input.empty()) {
			std::cout << "gpu starved.\n";
			_input_condition.wait(lm);
		}

		auto input = std::move(_input.front());
		_input.pop();
		lm.unlock();

		nvjpeg2kImageComponentInfo_t info[3];
		for (int i = 0; i < 3; ++i) {
			info[i].component_width = input.size().width;
			info[i].component_height = input.size().height;
			info[i].precision = 12;
			info[i].sgn = 0;
		}

		nvjpeg2kEncodeConfig_t config;
		memset(&config, 0, sizeof(config));
		config.stream_type = NVJPEG2K_STREAM_J2K;
		config.color_space = NVJPEG2K_COLORSPACE_SRGB;
		config.image_width = input.size().width;
		config.image_height = input.size().height;
		config.num_components = 3;
		config.image_comp_info = reinterpret_cast<nvjpeg2kImageComponentInfo_t*>(&info);
		config.code_block_w = 32;
		config.code_block_h = 32;
		config.irreversible = 0;
		config.mct_mode = 1;
		config.prog_order = NVJPEG2K_CPRL;
		config.num_resolutions = input.resolution() == Resolution::FOUR_K ? 7 : 6;
		config.enable_custom_precincts = 0;
		config.precint_width[0] = 7;
		config.precint_height[0] = 7;
		for (int i = 1; i < 6; ++i) {
			config.precint_width[i] = 8;
			config.precint_height[i] = 8;
		}

		auto status = nvjpeg2kEncodeParamsSetEncodeConfig(encoder_params, &config);
		if (status != NVJPEG2K_STATUS_SUCCESS) {
			throw CUDAError("nvjpeg2kEncodeParamsSetEncodeConfig", status);
		}

		// XXX: quality
		status = nvjpeg2kEncodeParamsSetQuality(encoder_params, 30);
		if (status != NVJPEG2K_STATUS_SUCCESS) {
			throw CUDAError("nvjpeg2kEncodeParamsSetQuality", status);
		}

		status = nvjpeg2kEncode(encoder_handle, encoder_state, encoder_params, input.device_image(), stream);
		if (status != NVJPEG2K_STATUS_SUCCESS) {
			throw CUDAError("nvjpeg2kEncode", status);
		}

		size_t compressed_size;
		status = nvjpeg2kEncodeRetrieveBitstream(encoder_handle, encoder_state, nullptr, &compressed_size, stream);

		dcp::ArrayData output(compressed_size);
		status = nvjpeg2kEncodeRetrieveBitstream(encoder_handle, encoder_state, output.data(), &compressed_size, stream);
		if (status != NVJPEG2K_STATUS_SUCCESS) {
			throw CUDAError("nvjpeg2kEncodeRetrieveBitstream", status);
		}

		boost::mutex::scoped_lock lm2(_output_mutex);
		_output[make_pair(input.index(), input.eyes())] = output;
		_output_condition.notify_all();
	}

	cudaStreamDestroy(stream);
}


CUDAJ2KFrameEncoder::Input::Input(DCPVideo const& vf)
	: _index(vf.index())
	, _eyes(vf.eyes())
	, _resolution(vf.resolution())
{
	_xyz = convert_to_xyz(vf.frame(), boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2));

	for (int i = 0; i < 3; ++i) {
		_pixel_data_h[i] = reinterpret_cast<uint8_t*>(_xyz->data(i));
	}

	_size = _xyz->size();

	auto const pitch = _size.width * 2;

	for (int i = 0; i < 3; ++i) {
		_pitch_in_bytes[i] = pitch;
		auto status = cudaMallocPitch(
			reinterpret_cast<void**>(&_pixel_data_d[i]),
			&_pitch_in_bytes[i],
			pitch,
			_size.height
			);

		if (status != cudaSuccess) {
			throw CUDAError("cudaMallocPitch", status);
		}

		status = cudaMemcpy2D(
			_pixel_data_d[i],
			_pitch_in_bytes[i],
			_pixel_data_h[i],
			_pitch_in_bytes[i],
			pitch,
			_size.height,
			cudaMemcpyHostToDevice
			);

		if (status != cudaSuccess) {
			throw CUDAError("cudaMemcpy2D", status);
		}
	}

	_device_image.num_components = 3;
	_device_image.pixel_data = reinterpret_cast<void**>(_pixel_data_d);
	_device_image.pixel_type = NVJPEG2K_UINT16;
	_device_image.pitch_in_bytes = reinterpret_cast<size_t*>(_pitch_in_bytes);
}


CUDAJ2KFrameEncoder::Input::Input(Input&& other)
	: _index(other._index)
	, _eyes(other._eyes)
	, _size(other._size)
	, _resolution(other._resolution)
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


CUDAJ2KFrameEncoder::Input::~Input()
{
	cudaFree(_pixel_data_d[0]);
	cudaFree(_pixel_data_d[1]);
	cudaFree(_pixel_data_d[2]);
}


optional<dcp::ArrayData>
CUDAJ2KFrameEncoder::encode(DCPVideo const& vf)
{
	auto input = Input(vf);

	auto const size = vf.frame()->out_size();
	DCPOMATIC_ASSERT(!_size || size == *_size);
	_size = size;

	DCPOMATIC_ASSERT(!_resolution || vf.resolution() == *_resolution);
	_resolution = vf.resolution();

	{
		boost::mutex::scoped_lock lm (_input_mutex);
		_input.push(std::move(input));
		std::cout << "push input: " << _input.size() << "\n";
		_input_condition.notify_all();
	}

	boost::mutex::scoped_lock lm(_output_mutex);
	while (_output.find(make_pair(vf.index(), vf.eyes())) == _output.end()) {
		_output_condition.wait(lm);
	}

	auto iter = _output.find(make_pair(vf.index(), vf.eyes()));
	auto data = iter->second;
	_output.erase(iter);
	return data;
}



void
CUDAJ2KFrameEncoder::log_thread_start ()
{
       LOG_TIMING("start-encoder-thread thread=%1", thread_id());
}


void
CUDAJ2KFrameEncoder::flush()
{

}

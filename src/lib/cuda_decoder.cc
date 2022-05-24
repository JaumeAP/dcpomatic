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


#include "cuda_decoder.h"
#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "image.h"
#include "scope_guard.h"
#include <dcp/openjpeg_image.h>
#include <nvjpeg2k.h>


using std::shared_ptr;
using std::string;


CUDADecoder* CUDADecoder::_instance = nullptr;


CUDADecoder::CUDADecoder()
{
	_thread = boost::thread(std::bind(&CUDADecoder::thread, this));
}


CUDADecoder::~CUDADecoder()
{
	try {
		_thread.interrupt();
		_thread.join();
	} catch (...) {}

	for (int i = 0; i < 3; ++i) {
		cudaFree(_device[i]);
	}
}


void
CUDADecoder::check_jpeg2k(string name, nvjpeg2kStatus_t status)
{
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError(name, status);
	}
}


void
CUDADecoder::thread()
try
{
	nvjpeg2kHandle_t handle;
	check_jpeg2k("nvjpeg2kCreateSimple", nvjpeg2kCreateSimple(&handle));
	ScopeGuard handle_guard([handle]() {
		nvjpeg2kDestroy(handle);
	});

	nvjpeg2kDecodeState_t state;
	check_jpeg2k("nvjpeg2kDecodeStateCreate", nvjpeg2kDecodeStateCreate(handle, &state));
	ScopeGuard state_guard([&state]() {
		nvjpeg2kDecodeStateDestroy(state);
	});

	nvjpeg2kStream_t jpeg2k_stream;
	check_jpeg2k("nvjpeg2kStreamCreate", nvjpeg2kStreamCreate(&jpeg2k_stream));
	ScopeGuard jpeg2k_stream_guard([&jpeg2k_stream]() {
		nvjpeg2kStreamDestroy(jpeg2k_stream);
	});

	while (true) {
		boost::mutex::scoped_lock lm(_mutex);
		while (_queue.empty()) {
			_queue_empty_condition.wait(lm);
		}

		auto input = std::move(_queue.front());
		_queue.pop();
		lm.unlock();

		auto output = decode_one(input, handle, state, jpeg2k_stream);

		lm.lock();
		_output[input.id] = output;
		_complete_condition.notify_all();
	}
}
catch (CUDAError& e)
{
	LOG_ERROR("CUDA error: %1 (aborting CUDADecoder)", e.what());
}
catch (boost::thread_interrupted&)
{

}
catch (std::exception& e)
{
	LOG_ERROR("Aborting CUDADecoder thread: %1", e.what());
}
catch (...)
{
	LOG_ERROR_NC("Aborting CUDADecoder thread: unknown error");
}



shared_ptr<Image>
CUDADecoder::decode_one(QueueItem const& input, nvjpeg2kHandle_t handle, nvjpeg2kDecodeState_t state, nvjpeg2kStream_t jpeg2k_stream)
{
	try {
		check_jpeg2k("nvjpeg2kStreamParse", nvjpeg2kStreamParse(handle, input.data->data(), input.data->size(), 0, 0, jpeg2k_stream));

		nvjpeg2kImageInfo_t image_info;
		check_jpeg2k("nvjpeg2kStreamGetImageInfo", nvjpeg2kStreamGetImageInfo(jpeg2k_stream, &image_info));

		nvjpeg2kImageComponentInfo_t image_component_info[3];
		for (int i = 0; i < 3; ++i) {
			check_jpeg2k("nvjpeg2kStreamGetImageComponentInfo", nvjpeg2kStreamGetImageComponentInfo(jpeg2k_stream, &image_component_info[i], i));
		}

		dcp::Size size(image_component_info[0].component_width, image_component_info[0].component_height);
		if (size != _allocation) {
			for (int i = 0; i < 3; ++i) {
				cudaFree(_device[i]);
				_device[i] = nullptr;
				auto status = cudaMallocPitch(reinterpret_cast<void**>(&_device[i]), &_pitch[i], size.width * 2, size.height);
				if (status != cudaSuccess) {
					throw CUDAError("cudaMallocPitch", status);
				}
				_host[i].resize(_pitch[i] * size.height / 2);
			}
			_allocation = size;
		}

		nvjpeg2kImage_t output_image;
		output_image.pixel_data = reinterpret_cast<void**>(_device);
		output_image.pixel_type = NVJPEG2K_UINT16;
		output_image.pitch_in_bytes = _pitch;
		output_image.num_components = 3;

		check_jpeg2k("nvjpeg2kDecode", nvjpeg2kDecode(handle, state, jpeg2k_stream, &output_image, 0));
		cudaDeviceSynchronize();

		for (int i = 0; i < 3; ++i) {
			auto status = cudaMemcpy(_host[i].data(), _device[i], _pitch[i] * size.height, cudaMemcpyDeviceToHost);
			if (status != cudaSuccess) {
				throw CUDAError("cudaMemcpy", status);
			}
		}

		auto output = std::make_shared<Image>(input.pixel_format, size, input.alignment);
		for (int y = 0; y < size.height; ++y) {
			int p = y * _pitch[0] / 2;
			auto q = reinterpret_cast<uint16_t *>(output->data()[0] + y * output->stride()[0]);
			for (int x = 0; x < size.width; ++x) {
				*q++ = _host[0][p] << 4;
				*q++ = _host[1][p] << 4;
				*q++ = _host[2][p] << 4;
				++p;
			}
		}

		return output;

	} catch (CUDAError& e) {
		LOG_ERROR("CUDA error: %1", e.what());
		return {};
	}
}


shared_ptr<Image>
CUDADecoder::decode(shared_ptr<const dcp::Data> j2k_data, int reduce, AVPixelFormat pixel_format, Image::Alignment alignment)
{
	boost::mutex::scoped_lock lm(_mutex);
	auto id = _next_id++;
	_queue.push({id, j2k_data, reduce, pixel_format, alignment});
	_queue_empty_condition.notify_all();

	while (_output.find(id) == _output.end()) {
		_complete_condition.wait(lm);
	}

	auto iter = _output.find(id);
	if (iter == _output.end()) {
		return {};
	}

	auto output = *iter;
	_output.erase(iter);
	return output.second;
}


CUDADecoder *
CUDADecoder::instance()
{
	if (!_instance) {
		_instance = new CUDADecoder();
	}

	return _instance;
}


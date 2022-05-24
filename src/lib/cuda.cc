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


#include "cuda.h"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include <dcp/openjpeg_image.h>
#include <nvjpeg2k.h>


using std::shared_ptr;


CUDA* CUDA::_instance = nullptr;


CUDA::CUDA()
{
	_decode_thread = std::thread(std::bind(&CUDA::decode_thread, this));
}


void
CUDA::decode_thread()
{
	nvjpeg2kHandle_t handle;
	auto status = nvjpeg2kCreateSimple(&handle);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kCreateSimple", status);
	}

	nvjpeg2kDecodeState_t decode_state;
	status = nvjpeg2kDecodeStateCreate(handle, &decode_state);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kDecodeStateCreate", status);
	}

	nvjpeg2kStream_t jpeg2k_stream;
	status = nvjpeg2kStreamCreate(&jpeg2k_stream);
	if (status != NVJPEG2K_STATUS_SUCCESS) {
		throw CUDAError("nvjpeg2kStreamCreate", status);
	}

	while (true) {
		boost::mutex::scoped_lock lm(_decode_mutex);
		while (_decode_queue.empty()) {
			_decode_queue_empty_condition.wait(lm);
		}

		auto input = std::move(_decode_queue.front());
		_decode_queue.pop();
		lm.unlock();

		try {
			std::cout << "we got " << input.data->size() << " bytes.\n";
			auto status = nvjpeg2kStreamParse(handle, input.data->data(), input.data->size(), 0, 0, jpeg2k_stream);
			if (status != NVJPEG2K_STATUS_SUCCESS) {
				throw CUDAError("nvjpeg2kStreamParse", status);
			}

			nvjpeg2kImageInfo_t image_info;
			status = nvjpeg2kStreamGetImageInfo(jpeg2k_stream, &image_info);
			if (status != NVJPEG2K_STATUS_SUCCESS) {
				throw CUDAError("nvjpeg2kStreamGetImageInfo", status);
			}
			std::cout << image_info.num_components << " components.\n";

			nvjpeg2kImageComponentInfo_t image_component_info[3];
			for (int i = 0; i < 3; ++i) {
				status = nvjpeg2kStreamGetImageComponentInfo(jpeg2k_stream, &image_component_info[i], i);
				if (status != NVJPEG2K_STATUS_SUCCESS) {
					throw CUDAError("nvjpeg2kStreamGetImageComponentInfo", status);
				}
			}

			auto const width = image_component_info[0].component_width;
			auto const height = image_component_info[0].component_height;
			std::cout << width << "x" << height << " " << ((int)image_component_info[0].precision) << "\n";

			uint16_t* decoded_d[3];
			size_t pitch[3];

			for (int i = 0; i < 3; ++i) {
				printf("cudaMallocPitch %d %d\n", width * 2, height);
				auto status = cudaMallocPitch(reinterpret_cast<void**>(&decoded_d[i]), &pitch[i], width * 2, height);
				if (status != cudaSuccess) {
					throw CUDAError("cudaMallocPitch", status);
				}
			}

			nvjpeg2kImage_t output_image;
			output_image.pixel_data = reinterpret_cast<void**>(decoded_d);
			output_image.pixel_type = NVJPEG2K_UINT16;
			output_image.pitch_in_bytes = pitch;
			output_image.num_components = 3;

			status = nvjpeg2kDecode(handle, decode_state, jpeg2k_stream, &output_image, 0);
			std::cout << "decode said " << status << "\n";
			if (status != NVJPEG2K_STATUS_SUCCESS) {
				abort();
				throw CUDAError("nvjpeg2kDecode", status);
			}
			cudaDeviceSynchronize();

			std::vector<uint16_t> decoded_h[3];
			for (int i = 0; i < 3; ++i) {
				auto size = pitch[i] * height;
				decoded_h[i].resize(size / 2);
				auto status = cudaMemcpy(decoded_h[i].data(), decoded_d[i], size, cudaMemcpyDeviceToHost);
				if (status != cudaSuccess) {
					throw CUDAError("cudaMemcpy", status);
				}
			}

			auto output = std::make_shared<Image>(input.pixel_format, dcp::Size(width, height), input.alignment);
			int p = 0;
			for (size_t y = 0; y < height; ++y) {
				auto q = reinterpret_cast<uint16_t *>(output->data()[0] + y * output->stride()[0]);
				for (size_t x = 0; x < width; ++x) {
					*q++ = decoded_h[0][p] << 4;
					*q++ = decoded_h[1][p] << 4;
					*q++ = decoded_h[2][p] << 4;
					++p;
				}
			}

			for (int i = 0; i < 3; ++i) {
				cudaFree(decoded_d[i]);
			}

			lm.lock();
			_decode_output[input.id] = output;
			_decode_complete_condition.notify_all();
			lm.unlock();
		} catch (CUDAError&) {
			lm.lock();
			_decode_output[input.id] = {};
			_decode_complete_condition.notify_all();
			lm.unlock();
		}
	}
}


shared_ptr<Image>
CUDA::decode(shared_ptr<const dcp::Data> j2k_data, int reduce, AVPixelFormat pixel_format, Image::Alignment alignment)
{
	boost::mutex::scoped_lock lm(_decode_mutex);
	auto id = _next_decode_id++;
	_decode_queue.push({id, j2k_data, reduce, pixel_format, alignment});
	_decode_queue_empty_condition.notify_all();

	while (_decode_output.find(id) == _decode_output.end()) {
		_decode_complete_condition.wait(lm);
	}

	auto iter = _decode_output.find(id);
	if (iter ==_decode_output.end()) {
		return {};
	}

	auto output = *iter;
	_decode_output.erase(iter);
	return output.second;
}


CUDA *
CUDA::instance()
{
	if (!_instance) {
		_instance = new CUDA();
	}

	return _instance;
}


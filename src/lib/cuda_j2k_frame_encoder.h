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


#ifndef DCPOMATIC_CUDA_J2K_FRAME_ENCODER
#define DCPOMATIC_CUDA_J2K_FRAME_ENCODER


#include "j2k_frame_encoder.h"
#include "types.h"
#include <dcp/types.h>
#include <nvjpeg2k.h>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <queue>
#include <thread>
#include <vector>


class CUDAJ2KFrameEncoder : public J2KFrameEncoder
{
public:
	CUDAJ2KFrameEncoder();

	boost::optional<dcp::ArrayData> encode(DCPVideo const &) override;
	void flush() override;
	void log_thread_start() override;

private:
	void encode_queue();

	class Input
	{
	public:
		Input(DCPVideo const& vf);
		Input(Input const& other) = delete;
		Input(Input&& other);
		~Input();

		Input& operator=(Input const& other) = delete;

		nvjpeg2kImage_t const* const device_image() const {
			return &_device_image;
		}

		int index() const {
			return _index;
		}

		Eyes eyes() const {
			return _eyes;
		}

		dcp::Size size() const {
			return _size;
		}

		Resolution resolution() const {
			return _resolution;
		}

	private:
		std::shared_ptr<dcp::OpenJPEGImage> _xyz;
		uint8_t* _pixel_data_h[3];
		uint8_t* _pixel_data_d[3];
		size_t _pitch_in_bytes[3];
		nvjpeg2kImage_t _device_image;
		int _index;
		Eyes _eyes;
		dcp::Size _size;
		Resolution _resolution;
	};

	boost::optional<dcp::Size> _size;
	boost::optional<Resolution> _resolution;

	static void cuda_thread();

	static std::vector<std::thread> _cuda_threads;
	static std::queue<Input> _input;
	static boost::condition _input_condition;
	static boost::mutex _input_mutex;

	static std::map<std::pair<int, Eyes>, dcp::ArrayData> _output;
	static boost::condition _output_condition;
	static boost::mutex _output_mutex;
};


#endif

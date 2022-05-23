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
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <vector>


class CUDAJ2KFrameEncoder : public J2KFrameEncoder
{
public:
	CUDAJ2KFrameEncoder();

	boost::optional<dcp::ArrayData> encode(DCPVideo const &) override;
	void flush() override;
	void log_thread_start() override;

	static int constexpr batch_size = 1;

private:
	void encode_queue();

	nvjpeg2kEncoder_t _encoder_handle;
	nvjpeg2kEncodeState_t _encoder_state;
	nvjpeg2kEncodeParams_t _encoder_params;

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

	private:
		uint8_t* _pixel_data_d[3];
		size_t _pitch_in_bytes[3];
		nvjpeg2kImage_t _device_image;
		int _index;
		Eyes _eyes;
	};

	static boost::mutex _mutex;
	static boost::condition _condition;
	static std::vector<Input> _queue;
	static std::map<std::pair<int, Eyes>, dcp::ArrayData> _output;

	static boost::optional<dcp::Size> _size;
	static boost::optional<Resolution> _resolution;
};


#endif

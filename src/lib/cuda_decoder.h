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


#include "image.h"
#include <dcp/array_data.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <nvjpeg2k.h>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <memory>
#include <queue>


class CUDADecoder
{
public:
	CUDADecoder();
	~CUDADecoder();

	CUDADecoder(CUDADecoder &) = delete;
	CUDADecoder(CUDADecoder &&) = delete;

	std::shared_ptr<Image> decode(std::shared_ptr<const dcp::Data> j2k_data, int reduce, AVPixelFormat pixel_format, Image::Alignment alignment);

	static CUDADecoder* instance();

private:
	typedef uint64_t ID;

	class QueueItem
	{
	public:
		ID id;
		std::shared_ptr<const dcp::Data> data;
		int reduce;
		AVPixelFormat pixel_format;
		Image::Alignment alignment;
	};

	void thread();
	std::shared_ptr<Image> decode_one(QueueItem const& input, nvjpeg2kHandle_t handle, nvjpeg2kDecodeState_t state, nvjpeg2kStream_t jpeg2k_stream);
	void check_jpeg2k(std::string name, nvjpeg2kStatus_t status);

	std::queue<QueueItem> _queue;
	std::map<ID, std::shared_ptr<Image>> _output;
	boost::condition _queue_empty_condition;
	boost::condition _complete_condition;
	boost::mutex _mutex;
	boost::thread _thread;
	ID _next_id = 0;
	dcp::Size _allocation;
	uint16_t* _device[3] = { nullptr, nullptr, nullptr };
	size_t _pitch[3];
	std::vector<uint16_t> _host[3];

	static CUDADecoder* _instance;
};


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
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <memory>
#include <queue>
#include <thread>


class CUDA
{
public:
	CUDA();

	CUDA(CUDA &) = delete;
	CUDA(CUDA &&) = delete;

	std::shared_ptr<Image> decode(std::shared_ptr<const dcp::Data> j2k_data, int reduce, AVPixelFormat pixel_format, Image::Alignment alignment);

	static CUDA* instance();

private:
	void decode_thread();

	typedef uint64_t ID;

	class DecodeQueueItem
	{
	public:
		ID id;
		std::shared_ptr<const dcp::Data> data;
		int reduce;
		AVPixelFormat pixel_format;
		Image::Alignment alignment;
	};

	std::queue<DecodeQueueItem> _decode_queue;
	std::map<ID, std::shared_ptr<Image>> _decode_output;
	boost::condition _decode_queue_empty_condition;
	boost::condition _decode_complete_condition;
	boost::mutex _decode_mutex;
	std::thread _decode_thread;
	ID _next_decode_id = 0;

	static CUDA* _instance;
};


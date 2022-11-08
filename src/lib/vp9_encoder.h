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


#include "dcpomatic_time.h"
#include "image_store.h"
#include "vp9_pcm_block.h"


struct AVCodecContext;


class VP9Encoder
{
public:
	VP9Encoder();

	VP9Encoder(VP9Encoder const&) = delete;
	VP9Encoder& operator=(VP9Encoder const&) = delete;

	~VP9Encoder();

	void encode(std::shared_ptr<const Image> image, dcpomatic::DCPTime time);

	dcpomatic::DCPTime end() const {
		return _end;
	}

private:
	friend int write_packet(void*, uint8_t const*, int);

	AVCodecContext* _codec_context = nullptr;

	ImageStore _pending_images;
	std::vector<uint8_t> _header;
	std::vector<std::pair<dcpomatic::DCPTime, VP9PCMBlock>> _pcm_blocks;
	dcpomatic::DCPTime _end;
};


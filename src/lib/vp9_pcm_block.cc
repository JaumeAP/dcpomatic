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


#include "constants.h"
#include "dcpomatic_assert.h"
#include "util.h"
#include "vp9_pcm_block.h"


using std::vector;


auto constexpr pcm_block_size = SIGN_LANGUAGE_AUDIO_FRAME_RATE * AUDIO_BYTES_PER_SAMPLE * SIGN_LANGUAGE_CHUNK_LENGTH_IN_SECONDS;


VP9PCMBlock::VP9PCMBlock(uint8_t const* vp9_segment, int vp9_segment_size, vector<uint8_t> const& ebml_header)
	: _data(pcm_block_size)
{
	uint32_t const ebml_header_size = ebml_header.size();

	DCPOMATIC_ASSERT(vp9_segment_size < static_cast<int>((pcm_block_size - 20 - ebml_header_size)));

	// H_1
	_data[0] = 0xff;
	_data[1] = 0xff;
	_data[2] = 0xff;
	_data[3] = 0xff;
	// Length of VP9 segment, big-endian, unsigned
	_data[4] = (vp9_segment_size >> 24) & 0xff;
	_data[5] = (vp9_segment_size >> 16) & 0xff;
	_data[6] = (vp9_segment_size >> 8) & 0xff;
	_data[7] = (vp9_segment_size >> 0) & 0xff;
	// Length of PCM block, big-endian, unsigned
	_data[8] = (pcm_block_size >> 24) & 0xff;
	_data[9] = (pcm_block_size >> 16) & 0xff;
	_data[10] = (pcm_block_size >> 8) & 0xff;
	_data[11] = (pcm_block_size >> 0) & 0xff;
	// Length of VP9 EBML header, big-endian, unsigned
	_data[12] = (ebml_header_size >> 24) & 0xff;
	_data[13] = (ebml_header_size >> 16) & 0xff;
	_data[14] = (ebml_header_size >> 8) & 0xff;
	_data[15] = (ebml_header_size >> 0) & 0xff;
	// H_2
	_data[16] = 0xff;
	_data[17] = 0xff;
	_data[18] = 0xff;
	_data[19] = 0xff;
	// VP9 EBML header
	std::copy(ebml_header.begin(), ebml_header.end(), _data.begin() + 20);
	// VP9 payload
	memcpy(_data.data() + 20 + ebml_header.size(), vp9_segment, vp9_segment_size);
}


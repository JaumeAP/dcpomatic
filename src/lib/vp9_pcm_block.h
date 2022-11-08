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


#ifndef DCPOMATIC_VP9_PCM_BLOCK_H
#define DCPOMATIC_VP9_PCM_BLOCK_H


#include <vector>
#include <stdint.h>


class VP9PCMBlock
{
public:
	VP9PCMBlock(uint8_t const* vp9_segment, int vp9_segment_size, std::vector<uint8_t> const& ebml_header);

private:
	std::vector<uint8_t> _data;
};


#endif


/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FRAME_INFO_H
#define DCPOMATIC_FRAME_INFO_H


#include "film.h"
#include <dcp/frame_info.h>
#include <memory>


class J2KFrameInfo : public dcp::J2KFrameInfo
{
public:
	J2KFrameInfo(dcp::J2KFrameInfo const& info);
	J2KFrameInfo(uint64_t offset_, uint64_t size_, std::vector<unsigned char> hash_);
	explicit J2KFrameInfo(std::shared_ptr<InfoFileHandle> info_file);
	J2KFrameInfo(std::shared_ptr<InfoFileHandle> info_file, Frame frame, Eyes eyes);

	void write(std::shared_ptr<InfoFileHandle> info_file, Frame frame, Eyes eyes) const;

	static int size_on_disk() {
		return _size_on_disk;
	}

private:
	void read(dcp::File& file);
	long position(Frame frame, Eyes eyes) const;

	static constexpr auto _size_on_disk = 16 + sizeof(dcp::J2KFrameInfo::offset) + sizeof(dcp::J2KFrameInfo::size);
};


#endif

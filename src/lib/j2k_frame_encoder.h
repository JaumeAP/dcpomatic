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


#ifndef DCPOMATIC_J2K_FRAME_ENCODER
#define DCPOMATIC_J2K_FRAME_ENCODER


#include <dcp/array_data.h>
#include <boost/optional.hpp>
#include <memory>


class DCPVideo;


class J2KFrameEncoder
{
public:
	virtual ~J2KFrameEncoder() {}

	virtual boost::optional<dcp::ArrayData> encode (DCPVideo const &) = 0;
	virtual void log_thread_start () = 0;
};


#endif


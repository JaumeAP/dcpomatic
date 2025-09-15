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


#include "digester.h"
#include "j2k_frame_info.h"
#include <dcp/file.h>
#include <boost/filesystem.hpp>


class InfoFileChecker
{
public:
	InfoFileChecker(dcp::File& file);

	void check(int64_t offset, uint8_t const* data, int length);

private:
	dcp::File& _file;
	boost::optional<J2KFrameInfo> _next;
	Digester _digester;
};


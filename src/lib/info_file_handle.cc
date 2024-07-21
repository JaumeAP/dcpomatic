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


#include "info_file_handle.h"
#include "exceptions.h"
#include <dcp/filesystem.h>



InfoFileHandle::InfoFileHandle(boost::mutex& mutex, boost::filesystem::path path, bool read)
	: _lock(mutex)
	, _file(path, read ? "rb" : (dcp::filesystem::exists(path) ? "r+b" : "wb"))
{
	if (!_file) {
		throw OpenFileError(path, errno, read ? OpenFileError::READ : (dcp::filesystem::exists(path) ? OpenFileError::READ_WRITE : OpenFileError::WRITE));
	}
}




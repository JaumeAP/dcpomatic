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


#include <boost/thread/mutex.hpp>
#include <dcp/file.h>


class InfoFileHandle
{
public:
	InfoFileHandle(boost::mutex& mutex, boost::filesystem::path file, bool read);

	dcp::File& get() {
		return _file;
	}

private:
	friend class Film;

	boost::mutex::scoped_lock _lock;
	dcp::File _file;
};


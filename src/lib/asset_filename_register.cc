/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "asset_filename_register.h"
#include "dcpomatic_assert.h"
#include <dcp/raw_convert.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>


using std::string;


boost::filesystem::path
AssetFilenameRegister::destination_filename(boost::filesystem::path source, string uuid)
{
	int tries = 0;
	boost::filesystem::path destination = source.filename();
	while (true) {
		auto iter = _used.find(destination);
		if (iter == _used.end()) {
			_used[destination] = uuid;
			return destination;
		}

		if (iter->second == uuid) {
			return destination;
		}

		/* Try to disambiguate filename */

		boost::smatch what;
		if (boost::regex_match(destination.stem().string(), what, boost::regex("(.*)_(\\d+)"))) {
			destination = fmt::format("{}_{}.mxf", what[1].str(), dcp::raw_convert<int>(what[2].str()) + 1);
		} else {
			destination = fmt::format("{}_0.mxf", destination.stem().string());
		}

		++tries;
		DCPOMATIC_ASSERT(tries < 100);
	}
}


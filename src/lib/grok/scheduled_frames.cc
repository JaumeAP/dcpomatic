/*
    Copyright (C) 2023 Grok Image Compression Inc.

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


#include "scheduled_frames.h"
#include "../dcp_video.h"


void
ScheduledFrames::store(DCPVideo const& val)
{
	std::unique_lock<std::mutex> lk(_mutex);
	auto it = _map.find(val.index());
	if (it == _map.end()) {
		_map.emplace(std::make_pair(val.index(), val));
	}
}


boost::optional<DCPVideo>
ScheduledFrames::retrieve(size_t index)
{
	std::unique_lock<std::mutex> lk(_mutex);
	auto it = _map.find(index);
	if (it == _map.end()) {
		return {};
	}

	DCPVideo val = it->second;
	_map.erase(index);

	return val;
}


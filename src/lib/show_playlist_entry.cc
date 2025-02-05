/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_content.h"
#include "dcpomatic_assert.h"
#include "show_playlist_entry.h"
#include <fmt/format.h>


using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;


ShowPlaylistEntry::ShowPlaylistEntry(shared_ptr<Content> content)
	: _content(content)
{

}


string
ShowPlaylistEntry::uuid() const
{
	if (auto dcp = dynamic_pointer_cast<DCPContent>(_content)) {
		DCPOMATIC_ASSERT(dcp->cpl());
		return *dcp->cpl();
	} else {
		return _content->digest();
	}
}


string
ShowPlaylistEntry::name() const
{
	if (auto dcp = dynamic_pointer_cast<DCPContent>(_content)) {
		return dcp->name();
	} else {
		return _content->path(0).filename().string();
	}
}


dcp::ContentKind
ShowPlaylistEntry::kind() const
{
	if (auto dcp = dynamic_pointer_cast<DCPContent>(_content)) {
		return dcp->content_kind().get_value_or(dcp::ContentKind::FEATURE);
	} else {
		return dcp::ContentKind::FEATURE;
	}
}


bool
ShowPlaylistEntry::encrypted() const
{
	if (auto dcp = dynamic_pointer_cast<DCPContent>(_content)) {
		return dcp->encrypted();
	} else {
		return false;
	}
}


string
ShowPlaylistEntry::approximate_length() const
{
	auto const hmsf = _content->approximate_length().split(24);
	return fmt::format("{:02d}:{:02d}:{:02d}", hmsf.h, hmsf.m, hmsf.s);
}


nlohmann::json
ShowPlaylistEntry::as_json() const
{
	nlohmann::json json;
	json["uuid"] = uuid();
	json["name"] = name();
	json["kind"] = kind().name();
	json["encrypted"] = encrypted();
	return json;
}


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


#include "dcpomatic_assert.h"
#include "video_type.h"
#include <string>


using std::string;


string
video_type_to_string(VideoType type)
{
	switch (type) {
	case VideoType::MAIN:
		return "main";
	case VideoType::SIGN_LANGUAGE:
		return "sign-language";
	default:
		DCPOMATIC_ASSERT(false);
	}
}


VideoType
string_to_video_type(string type)
{
	if (type == "main") {
		return VideoType::MAIN;
	} else if (type == "sign-language") {
		return VideoType::SIGN_LANGUAGE;
	}

	DCPOMATIC_ASSERT(false);
}


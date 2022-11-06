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


#ifndef DCPOMATIC_VIDEO_TYPE_H
#define DCPOMATIC_VIDEO_TYPE_H


#include <string>
#include <vector>


enum class VideoType
{
	MAIN,
	SIGN_LANGUAGE,
	COUNT
};


VideoType string_to_video_type(std::string type);
std::string video_type_to_string(VideoType type);


std::vector<VideoType> video_types();


#endif

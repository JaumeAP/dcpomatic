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


#ifndef DCPOMATIC_EMPTY_VIDEO_H
#define DCPOMATIC_EMPTY_VIDEO_H


#include "content_part.h"
#include "dcpomatic_time.h"
#include "playlist.h"
#include <list>
#include <utility>


struct empty_video_test1;
struct empty_video_test2;
struct empty_video_test3;
struct empty_video_test_with_overlapping_content;
struct empty_video_test_with_three_d1;
struct empty_video_test_with_three_d2;
struct player_subframe_test;


class EmptyVideo
{
public:
	EmptyVideo () {}
	EmptyVideo (std::shared_ptr<const Film> film, std::shared_ptr<const Playlist> playlist, dcpomatic::DCPTime length);

	dcpomatic::DCPTime position () const {
		return _position;
	}

	std::pair<dcpomatic::DCPTimePeriod, Eyes> period_at_position () const;

	bool done () const;

	void set_position (dcpomatic::DCPTime amount);

private:
	friend struct ::empty_video_test1;
	friend struct ::empty_video_test2;
	friend struct ::empty_video_test3;
	friend struct ::empty_video_test_with_overlapping_content;
	friend struct empty_video_test_with_three_d1;
	friend struct empty_video_test_with_three_d2;
	friend struct ::player_subframe_test;

	std::list<std::pair<dcpomatic::DCPTimePeriod, Eyes>> _periods;
	dcpomatic::DCPTime _position;
};


#endif

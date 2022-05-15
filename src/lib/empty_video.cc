/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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


#include "content.h"
#include "content_part.h"
#include "dcp_content.h"
#include "dcpomatic_time_coalesce.h"
#include "empty_video.h"
#include "film.h"
#include "piece.h"
#include "playlist.h"
#include "video_content.h"
#include <iostream>


using std::cout;
using std::dynamic_pointer_cast;
using std::function;
using std::list;
using std::make_pair;
using std::pair;
using std::shared_ptr;
using namespace dcpomatic;


EmptyVideo::EmptyVideo (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist, DCPTime length)
{
	list<DCPTimePeriod> full_left;
	list<DCPTimePeriod> full_right;
	list<DCPTimePeriod> full_both;
	for (auto i: playlist->content()) {
		if (i->video && i->video->use() && i->can_be_played() && i->paths_valid()) {
			auto period = DCPTimePeriod(i->position(), i->end(film));
			if (i->video->frame_type() == VideoFrameType::THREE_D_LEFT) {
				full_left.push_back (period);
			} else if (i->video->frame_type() == VideoFrameType::THREE_D_RIGHT) {
				full_right.push_back (period);
			} else {
				full_both.push_back (period);
			}
		}
	}

	auto const whole = DCPTimePeriod(DCPTime(), length);

	auto empty_both = subtract(subtract(subtract({whole}, full_left), full_right), full_both);
	auto empty_left = subtract(subtract(subtract({whole}, full_left), full_both), empty_both);
	auto empty_right = subtract(subtract(subtract({whole}, full_right), full_both), empty_both);

	for (auto left: empty_left) {
		_periods.push_back (make_pair(left, Eyes::LEFT));
	}

	for (auto right: empty_right) {
		_periods.push_back (make_pair(right, Eyes::RIGHT));
	}

	for (auto both: empty_both) {
		_periods.push_back (make_pair(both, Eyes::BOTH));
	}

	_periods.sort([](std::pair<DCPTimePeriod, Eyes> const& a, std::pair<DCPTimePeriod, Eyes> const& b) {
		return a.first < b.first;
	});

	if (!_periods.empty()) {
		_position = _periods.front().first.from;
	}
}


void
EmptyVideo::set_position (DCPTime position)
{
	_position = position;

	for (auto i: _periods) {
		if (i.first.contains(_position)) {
			return;
		}
	}

	for (auto i: _periods) {
		if (i.first.from > _position) {
			_position = i.first.from;
			return;
		}
	}
}


pair<DCPTimePeriod, Eyes>
EmptyVideo::period_at_position () const
{
	for (auto i: _periods) {
		if (i.first.contains(_position)) {
			return make_pair(DCPTimePeriod(_position, i.first.to), i.second);
		}
	}

	DCPOMATIC_ASSERT (false);
}


bool
EmptyVideo::done () const
{
	DCPTime latest;
	for (auto i: _periods) {
		latest = max (latest, i.first.to);
	}

	return _position >= latest;
}

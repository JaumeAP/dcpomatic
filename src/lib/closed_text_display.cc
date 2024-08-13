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


#include "butler.h"
#include "closed_text_display.h"
#include "constants.h"
#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include "string_text.h"
#include "text_ring_buffers.h"


using std::weak_ptr;
using boost::optional;


void
ClosedTextDisplay::set_butler(weak_ptr<Butler> butler)
{
	_butler = butler;
}


class ClosedTextSorter
{
public:
	bool operator() (StringText const & a, StringText const & b)
	{
		return from_top(a) < from_top(b);
	}

private:
	float from_top (StringText const & c) const
	{
		switch (c.v_align()) {
		case dcp::VAlign::TOP:
			return c.v_position();
		case dcp::VAlign::CENTER:
			return c.v_position() + 0.5;
		case dcp::VAlign::BOTTOM:
			return 1.0 - c.v_position();
		}
		DCPOMATIC_ASSERT (false);
		return 0;
	}
};


bool
ClosedTextDisplay::update(dcpomatic::DCPTime time, optional<DCPTextTrack> track)
{
	bool changed = false;

	/* Clear _current if it should not be visible */
	if (!_current.empty() && _current_period && !_current_period->contains(time)) {
		_current = {};
		_current_period = {};
		changed = true;
	}

	/* Fill next if necessary and possible */
	if (_next.empty() && track) {
		if (auto butler = _butler.lock()) {
			optional<TextRingBuffers::Data> data;
			while (auto d = butler->get_closed_caption()) {
				if (d->track == *track && d->period.to > time) {
					data = d;
					break;
				}
			}

			if (data) {
				auto to_show = data->text.string;
				std::sort(to_show.begin(), to_show.end(), ClosedTextSorter());

				auto j = to_show.begin();
				int k = 0;
				while (j != to_show.end() && k < MAX_CLOSED_CAPTION_LINES) {
					_next.push_back(j->text());
					++j;
					++k;
				}

				_next_period = data->period;
			}
		}
	}

	/* Swap next in if it's time */
	if (_next_period && _next_period->contains(time)) {
		_current = _next;
		_current_period = _next_period;
		_next = {};
		_next_period = {};
		changed = true;
	}

	return changed;
}


void
ClosedTextDisplay::clear()
{
	_current = {};
	_current_period = {};
	_next = {};
	_next_period = {};
}


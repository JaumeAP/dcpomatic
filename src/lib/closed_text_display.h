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


#include "dcpomatic_time.h"
#include <memory>
#include <string>
#include <vector>


class Butler;
class DCPTextTrack;
class Film;


/** @class ClosedTextDisplay
 *  @brief Helper for displaying closed text from a butler.
 *
 *  This class extracts the logic used by the closed text dialogue box,
 *  mostly to make it more easily testable.
 */
class ClosedTextDisplay
{
public:
	void clear();
	void set_butler(std::weak_ptr<Butler> butler);

	/** Set up the display for a particular time and text track */
	bool update(dcpomatic::DCPTime time, boost::optional<DCPTextTrack> track);

	/** @return the current closed text lines that should be shown */
	std::vector<std::string> const& current() const {
		return _current;
	}

private:
	std::weak_ptr<Butler> _butler;
	std::vector<std::string> _current;
	boost::optional<dcpomatic::DCPTimePeriod> _current_period;
	std::vector<std::string> _next;
	boost::optional<dcpomatic::DCPTimePeriod> _next_period;
};


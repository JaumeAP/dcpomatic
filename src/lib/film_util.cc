/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "film.h"
#include "film_util.h"


using std::shared_ptr;


bool
channel_is_mapped(shared_ptr<const Film> film, dcp::Channel channel)
{
	auto const mapped = film->mapped_audio_channels();
	return std::find(mapped.begin(), mapped.end(), static_cast<int>(channel)) != mapped.end();
}


int
audio_channels_to_write_to_mxf(shared_ptr<const Film> film)
{
	if (film->interop()) {
		return film->audio_channels();
	}

	/* Compromise to try and avoid validation errors in some cases with DCPs that
	 * don't have 8 or 16 channels (see bug #2443).
	 */
	return film->audio_channels() > 8 ? 16 : 8;
}


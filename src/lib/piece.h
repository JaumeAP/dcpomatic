/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_PIECE_H
#define DCPOMATIC_PIECE_H

#include "content.h"
#include "dcpomatic_time.h"
#include "frame_rate_change.h"
#include "types.h"

class Content;
class Decoder;

class Piece
{
public:
	Piece (boost::shared_ptr<Content> c, boost::shared_ptr<Decoder> d, FrameRateChange f)
		: content (c)
		, decoder (d)
		, frc (f)
		, done (false)
	{}

	dcpomatic::DCPTime position () const {
		return content->position ();
	}

	dcpomatic::DCPTime end (boost::shared_ptr<const Film> film) const {
		return content->end (film);
	}

	boost::shared_ptr<Content> content;
	boost::shared_ptr<Decoder> decoder;
	FrameRateChange frc;
	bool done;
};

#endif

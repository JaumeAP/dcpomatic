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
#include "video_content.h"

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

	dcpomatic::DCPTime content_video_to_dcp (Frame f) const
	{
		/* It might seem more logical here to convert s to a ContentTime (using the FrameRateChange)
		   then convert that ContentTime to frames at the content's rate.  However this fails for
		   situations like content at 29.9978733fps, DCP at 30fps.  The accuracy of the Time type is not
		   enough to distinguish between the two with low values of time (e.g. 3200 in Time units).

		   Instead we convert the DCPTime using the DCP video rate then account for any skip/repeat.
		   */
		dcpomatic::DCPTime const d = dcpomatic::DCPTime::from_frames(f * frc.factor(), frc.dcp) - dcpomatic::DCPTime(content->trim_start(), frc);
		return d + position();
	}

	dcpomatic::DCPTime content_time_to_dcp (dcpomatic::ContentTime t) const
	{
		return max (dcpomatic::DCPTime(), dcpomatic::DCPTime(t - content->trim_start(), frc) + position());
	}

	Crop video_crop () const {
		return content->video->crop ();
	}

	boost::shared_ptr<Content> content;
	boost::shared_ptr<Decoder> decoder;
	FrameRateChange frc;
	bool done;
};

#endif

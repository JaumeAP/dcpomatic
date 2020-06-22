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

#include "piece.h"


using boost::shared_ptr;
using namespace dcpomatic;


DCPTime
Piece::position () const
{
	return content->position ();
}


DCPTime
Piece::end (shared_ptr<const Film> film) const
{
	return content->end (film);
}


DCPTime
Piece::content_video_to_dcp (Frame f) const
{
	/* It might seem more logical here to convert s to a ContentTime (using the FrameRateChange)
	   then convert that ContentTime to frames at the content's rate.  However this fails for
	   situations like content at 29.9978733fps, DCP at 30fps.  The accuracy of the Time type is not
	   enough to distinguish between the two with low values of time (e.g. 3200 in Time units).

	   Instead we convert the DCPTime using the DCP video rate then account for any skip/repeat.
	   */
	DCPTime const d = DCPTime::from_frames(f * frc.factor(), frc.dcp) - DCPTime(content->trim_start(), frc);
	return d + position();
}


DCPTime
Piece::content_time_to_dcp (ContentTime t) const
{
	return max (DCPTime(), DCPTime(t - content->trim_start(), frc) + position());
}


Crop
Piece::video_crop () const
{
	return content->video->crop ();
}


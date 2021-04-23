/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_content.h"
#include "content.h"
#include "film.h"
#include "piece.h"
#include "video_content.h"


using std::shared_ptr;
using namespace dcpomatic;


Piece::Piece (shared_ptr<Content> c, shared_ptr<Decoder> d, FrameRateChange f)
	: content (c)
	, decoder (d)
	, frc (f)
{
	if (content->audio) {
		for (auto j: content->audio->streams()) {
			_stream_last_push_end[j] = content->position();
		}
	}
}



void
Piece::update_pull_to (DCPTime& pull_to) const
{
	if (done) {
		return;
	}

	for (auto const& i: _stream_last_push_end) {
		pull_to = std::min(pull_to, i.second);
	}
}


void
Piece::set_last_push_end (AudioStreamPtr stream, DCPTime end)
{
	DCPOMATIC_ASSERT (_stream_last_push_end.find(stream) != _stream_last_push_end.end());
	_stream_last_push_end[stream] = end;
}


DCPTime
Piece::content_video_to_dcp (Frame f) const
{
	/* See comment in resampled_audio_to_dcp */
	auto const d = DCPTime::from_frames(f * frc.factor(), frc.dcp) - DCPTime(content->trim_start(), frc);
	return d + content->position();
}


DCPTime
Piece::resampled_audio_to_dcp (Frame f, shared_ptr<const Film> film) const
{
	/* It might seem more logical here to convert s to a ContentTime (using the FrameRateChange)
	   then convert that ContentTime to frames at the content's rate.  However this fails for
	   situations like content at 29.9978733fps, DCP at 30fps.  The accuracy of the Time type is not
	   enough to distinguish between the two with low values of time (e.g. 3200 in Time units).

	   Instead we convert the DCPTime using the DCP video rate then account for any skip/repeat.
	*/
	return DCPTime::from_frames(f, film->audio_frame_rate())
		- DCPTime(content->trim_start(), frc)
		+ content->position();
}


ContentTime
Piece::dcp_to_content_time (DCPTime t, shared_ptr<const Film> film) const
{
	auto s = t - content->position ();
	s = min (content->length_after_trim(film), s);
	return max (ContentTime(), ContentTime(s, frc) + content->trim_start());
}


DCPTime
Piece::content_time_to_dcp (ContentTime t) const
{
	return max (DCPTime(), DCPTime(t - content->trim_start(), frc) + content->position());
}


bool
Piece::use_video () const
{
	return content->video && content->video->use();
}


dcpomatic::DCPTime
Piece::position () const
{
	return content->position ();
}


dcpomatic::DCPTime
Piece::end (shared_ptr<const Film> film) const
{
	return content->end (film);
}


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


#include "audio_content.h"
#include "film.h"
#include "piece.h"
#include "text_content.h"


using std::list;
using std::vector;
using boost::optional;
using boost::shared_ptr;
using namespace dcpomatic;


DCPTime
Piece::position () const
{
	return _content->position ();
}


DCPTime
Piece::end (shared_ptr<const Film> film) const
{
	return _content->end (film);
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
	DCPTime const d = DCPTime::from_frames(f * frc.factor(), frc.dcp) - DCPTime(_content->trim_start(), frc);
	return d + position();
}


DCPTime
Piece::content_time_to_dcp (ContentTime t) const
{
	return max (DCPTime(), DCPTime(t - _content->trim_start(), frc) + position());
}


optional<DCPTime>
Piece::content_time_to_dcp (shared_ptr<const Content> c, ContentTime t) const
{
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (i == c) {
			return i->content_time_to_dcp (t);
		}
	}

	return optional<DCPTime>();
}


ContentTime
Piece::trim_start () const
{
	return _content[0]->trim_start ();
}


list<shared_ptr<Font> >
Piece::fonts () const
{
	list<shared_ptr<Font> > fonts;
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
			/* XXX: things may go wrong if there are duplicate font IDs
			   with different font files.
			*/
			list<shared_ptr<Font> > f = j->fonts ();
			copy (f.begin(), f.end(), back_inserter (fonts));
		}
	}
}


Crop
Piece::video_crop () const
{
	return _content->video->crop ();
}


DCPTime
Piece::resampled_audio_to_dcp (shared_ptr<const Film> film, Frame f) const
{
	/* See notes in content_video_to_dcp */
	return DCPTime::from_frames(f, film->audio_frame_rate())
		- DCPTime(_content->trim_start(), frc)
		+ position();
}


ContentTime
Piece::dcp_to_content_time (shared_ptr<const Film> film, DCPTime t) const
{
	DCPTime s = t - position();
	s = min(_content->length_after_trim(film), s);
	return max(ContentTime(), ContentTime(s, frc) + _content->trim_start());
}


bool
Piece::video_use () const
{
	return _content->video->use();
}


optional<double>
Piece::video_fade (shared_ptr<const Film> film, Frame frame) const
{
	return _content->video->fade (film, frame);
}


dcp::Size
Piece::video_scaled_size (dcp::Size container_size)
{
	return _content->video->scaled_size(container_size);
}


optional<ColourConversion>
Piece::video_colour_conversion () const
{
	return _content->video->colour_conversion();
}


VideoRange
Piece::video_range () const
{
	return _content->video->range();
}


int
Piece::audio_resampled_frame_rate (boost::shared_ptr<const Film> film) const
{
	return _content->audio->resampled_frame_rate (film);
}


double
Piece::audio_gain () const
{
	return _content->audio->gain();
}


vector<AudioStreamPtr>
Piece::audio_streams () const
{
	vector<AudioStreamPtr> streams;
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (i->audio) {
			vector<AudioStreamPtr> s = i->audio->streams ();
			copy (s.begin(), s.end(), back_inserter(streams));
		}
	}

	return streams;
}


bool
Piece::referenced_audio () const
{
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(_content[0]);
	return dcp && dcp->reference_audio();
}


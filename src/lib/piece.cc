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
#include "audio_stream.h"
#include "content.h"
#include "dcp_content.h"
#include "decoder.h"
#include "piece.h"
#include "text_content.h"
#include "video_content.h"
#include <boost/foreach.hpp>


using std::copy;
using std::list;
using std::map;
using boost::dynamic_pointer_cast;
using boost::optional;
using boost::shared_ptr;
using namespace dcpomatic;


Piece::Piece (shared_ptr<Content> c, shared_ptr<Decoder> d, FrameRateChange f)
	: _frc (f)
	, _done (false)
{
	_content.push_back (c);
	_decoder.push_back (d);

	if (c->audio) {
		BOOST_FOREACH (AudioStreamPtr i, c->audio->streams()) {
			_stream_last_push_end[i] = c->position();
		}
	}
}


void
Piece::update_pull_to (DCPTime& pull_to) const
{
	if (_done) {
		return;
	}

	for (map<AudioStreamPtr, DCPTime>::const_iterator i = _stream_last_push_end.begin(); i != _stream_last_push_end.end(); ++i) {
		if (i->second < pull_to) {
			pull_to = i->second;
		}
	}
}


void
Piece::set_last_push_end (AudioStreamPtr stream, DCPTime end)
{
	DCPOMATIC_ASSERT (_stream_last_push_end.find(stream) != _stream_last_push_end.end());
	_stream_last_push_end[stream] = end;
}


DCPTime
Piece::position () const
{
	return _content[0]->position();
}


DCPTime
Piece::end (shared_ptr<const Film> film) const
{
	return _content[0]->end(film);
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

	DCPTime const d = DCPTime::from_frames(f * _frc.factor(), _frc.dcp) - DCPTime(_content[0]->trim_start(), _frc);
	return d + position();
}


DCPTime
Piece::resampled_audio_to_dcp (shared_ptr<const Film> film, Frame f) const
{
	/* See comment in dcp_to_content_video */
	return DCPTime::from_frames(f, film->audio_frame_rate()) - DCPTime(_content[0]->trim_start(), _frc) + position();
}


ContentTime
Piece::dcp_to_content_time (shared_ptr<const Film> film, DCPTime t) const
{
	DCPTime s = t - position();
	s = min (_content[0]->length_after_trim(film), s);
	return max (ContentTime(), ContentTime(s, _frc) + _content[0]->trim_start());
}


DCPTime
Piece::content_time_to_dcp (ContentTime t) const
{
	return max (DCPTime(), DCPTime(t - _content[0]->trim_start(), _frc) + position());
}


void
Piece::add_fonts (list<shared_ptr<Font> >& fonts) const
{
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
			/* XXX: things may go wrong if there are duplicate font IDs
			   with different font files.
			   */
			list<shared_ptr<Font> > f = j->fonts ();
			copy (f.begin(), f.end(), back_inserter(fonts));
		}
	}
}


optional<DCPTime>
Piece::decoder_position (shared_ptr<const Film> film)
{
	if (_done) {
		return optional<DCPTime>();
	}

	DCPTime const t = content_time_to_dcp (max(_decoder[0]->position(), _content[0]->trim_start()));
	if (t > end(film)) {
		_done = true;
		return optional<DCPTime>();
	}

	return t;
}


bool
Piece::has_text () const
{
	return !_decoder[0]->text.empty();
}


void
Piece::pass ()
{
	_done = _decoder[0]->pass();
}


bool
Piece::reference_audio () const
{
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(_content[0]);
	return dcp && dcp->reference_audio();
}


bool
Piece::video_use () const
{
	return _content[0]->video && _content[0]->video->use();
}


Crop
Piece::video_crop () const
{
	return _content[0]->video->crop();
}


optional<double>
Piece::video_fade (boost::shared_ptr<const Film> film, Frame frame) const
{
	return _content[0]->video->fade(film, frame);
}


optional<ColourConversion>
Piece::video_colour_conversion () const
{
	return _content[0]->video->colour_conversion();
}


VideoRange
Piece::video_range () const
{
	return _content[0]->video->range();
}


dcp::Size
Piece::video_scaled_size (dcp::Size container_size) const
{
	return _content[0]->video->scaled_size(container_size);
}


int
Piece::audio_resampled_frame_rate (shared_ptr<const Film> film) const
{
	return _content[0]->audio->resampled_frame_rate(film);
}


double
Piece::audio_gain () const
{
	return _content[0]->audio->gain();
}


void
Piece::seek (shared_ptr<const Film> film, DCPTime time, bool accurate)
{
	if (time < position()) {
		/* Before; seek to the start of the content.  Even if this request is for an inaccurate seek
		   we must seek this (following) content accurately, otherwise when we come to the end of the current
		   content we may not start right at the beginning of the next, causing a gap (if the next content has
		   been trimmed to a point between keyframes, or something).
		   */
		_decoder[0]->seek (dcp_to_content_time(film, position()), true);
		_done = false;
	} else if (position() <= time && time < end(film)) {
		/* During; seek to position */
		_decoder[0]->seek (dcp_to_content_time(film, time), accurate);
		_done = false;
	} else {
		/* After; this piece is done */
		_done = true;
	}
}


optional<DCPTime>
Piece::content_time_to_dcp (shared_ptr<Content> content, ContentTime t) const
{
	if (content != _content[0]) {
		return optional<DCPTime>();
	}

	return content_time_to_dcp (t);
}


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


#include "audio_stream.h"
#include "content.h"
#include "piece.h"
#include <boost/foreach.hpp>


using std::copy;
using std::list;
using boost::shared_ptr;


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
	return DCPTime::from_frames(f, film->audio_frame_rate()) - DCPTime(_content[0]->trim_start(), frc) + position();
}


ContentTime
Piece::dcp_to_content_time (shared_ptr<const Film> film, DCPTime t) const
{
	DCPTime s = t - position();
	s = min (_content[0]->length_after_trim(_film), s);
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
Piece::position (shared_ptr<const Film> film)
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
	return !_decoder[0].text.empty();
}


void
Piece::pass () const
{
	_done = _decoder[0]->pass();
}


bool
Piece::reference_audio () const
{
	shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(_content[0]);
	return dcp && dcp->reference_audio();
}

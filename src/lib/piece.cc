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
#include "dcp_content.h"
#include "dcpomatic_log.h"
#include "decoder.h"
#include "film.h"
#include "piece.h"
#include "piece_video.h"
#include "player_video.h"
#include "video_content.h"
#include "video_decoder.h"


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;
using boost::optional;
using namespace dcpomatic;


Piece::Piece (weak_ptr<const Film> film, shared_ptr<Content> c, shared_ptr<Decoder> d, FrameRateChange f)
	: _film (film)
	, _content (c)
	, _decoder (d)
	, _frc (f)
{
	if (_content->audio) {
		for (auto j: _content->audio->streams()) {
			_stream_last_push_end[j] = _content->position();
		}
	}

	if (_decoder->video) {
		_decoder->video->Data.connect (boost::bind(&Piece::video, this, _1, _2, _3, _4));
	}
}


void
Piece::video (shared_ptr<const ImageProxy> image, Frame frame, Eyes eyes, Part part)
{
	Video (PieceVideo(image, frame, eyes, part));
}


void
Piece::update_pull_to (DCPTime& pull_to) const
{
	if (_done) {
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
	auto const d = DCPTime::from_frames(f * _frc.factor(), _frc.dcp) - DCPTime(_content->trim_start(), _frc);
	return d + _content->position();
}


DCPTime
Piece::resampled_audio_to_dcp (Frame f) const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	/* It might seem more logical here to convert s to a ContentTime (using the FrameRateChange)
	   then convert that ContentTime to frames at the content's rate.  However this fails for
	   situations like content at 29.9978733fps, DCP at 30fps.  The accuracy of the Time type is not
	   enough to distinguish between the two with low values of time (e.g. 3200 in Time units).

	   Instead we convert the DCPTime using the DCP video rate then account for any skip/repeat.
	*/
	return DCPTime::from_frames(f, film->audio_frame_rate())
		- DCPTime(_content->trim_start(), _frc)
		+ _content->position();
}


ContentTime
Piece::dcp_to_content_time (DCPTime t) const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	auto s = t - _content->position ();
	s = min (_content->length_after_trim(film), s);
	return max (ContentTime(), ContentTime(s, _frc) + _content->trim_start());
}


optional<DCPTime>
Piece::content_time_to_dcp (shared_ptr<const Content> content, ContentTime t) const
{
	if (_content != content) {
		return {};
	}

	return max (DCPTime(), DCPTime(t - _content->trim_start(), _frc) + _content->position());
}


bool
Piece::use_video () const
{
	return _content->video && _content->video->use();
}


VideoFrameType
Piece::video_frame_type () const
{
	DCPOMATIC_ASSERT (_content->video);
	return _content->video->frame_type ();
}


dcpomatic::DCPTime
Piece::position () const
{
	return _content->position ();
}


dcpomatic::DCPTime
Piece::end () const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return _content->end (film);
}


shared_ptr<PlayerVideo>
Piece::player_video (PieceVideo video, dcp::Size container_size) const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	return std::make_shared<PlayerVideo>(
		video.image,
		_content->video->crop (),
		_content->video->fade (film, video.frame),
		scale_for_display(_content->video->scaled_size(film->frame_size()), container_size, film->frame_size()),
		container_size,
		video.eyes,
		video.part,
		_content->video->colour_conversion(),
		_content->video->range(),
		_content,
		video.frame,
		false
		);
}


int
Piece::resampled_audio_frame_rate () const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	DCPOMATIC_ASSERT (_content->audio);
	return _content->audio->resampled_frame_rate (film);
}


double
Piece::audio_gain () const
{
	DCPOMATIC_ASSERT (_content->audio);
	return _content->audio->gain();
}


shared_ptr<Decoder>
Piece::decoder_for (shared_ptr<Content> content) const
{
	if (content == _content) {
		return _decoder;
	}

	return {};
}


void
Piece::pass ()
{
	LOG_DEBUG_PLAYER ("Calling pass() on %1", _content->path(0));
	_done = _decoder->pass();
}


bool
Piece::reference_dcp_audio () const
{
	auto dcp = dynamic_pointer_cast<DCPContent>(_content);
	return dcp && dcp->reference_audio();
}


void
Piece::seek (DCPTime time, bool accurate)
{
	if (time < position()) {
		/* Before; seek to the start of the content.  Even if this request is for an inaccurate seek
		   we must seek this (following) content accurately, otherwise when we come to the end of the current
		   content we may not start right at the beginning of the next, causing a gap (if the next content has
		   been trimmed to a point between keyframes, or something).
		   */
		_decoder->seek (dcp_to_content_time(position()), true);
		_done = false;
	} else if (position() <= time && time < end()) {
		/* During; seek to position */
		_decoder->seek (dcp_to_content_time(time), accurate);
		_done = false;
	} else {
		/* After; this piece is done */
		_done = true;
	}
}


optional<dcpomatic::DCPTime>
Piece::decoder_before(optional<dcpomatic::DCPTime> time)
{
	if (_done) {
		return {};
	}

	auto t = content_time_to_dcp(_content, std::max(_decoder->position(), _content->trim_start()));
	DCPOMATIC_ASSERT (t);

	if (*t > end()) {
		_done = true;
	} else {
		/* Given two choices at the same time, pick the one with texts so we see it before
		   the video.
		   */
		if (!time || t < *time || (t == *time && !_decoder->text.empty())) {
			return t;
		}
	}

	return {};
}

vector<dcpomatic::FontData>
Piece::fonts () const
{
	return _decoder->fonts();
}


bool
Piece::ignore_video_at (DCPTime time) const
{
	return _ignore_video && _ignore_video->contains(time);
}


DCPTimePeriod
Piece::period () const
{
	return DCPTimePeriod(position(), end());
}


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
#include "player_video.h"
#include "video_content.h"


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using boost::optional;
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


optional<DCPTime>
Piece::content_time_to_dcp (shared_ptr<const Content> content_, ContentTime t) const
{
	if (content != content_) {
		return {};
	}

	return max (DCPTime(), DCPTime(t - content->trim_start(), frc) + content->position());
}


bool
Piece::use_video () const
{
	return content->video && content->video->use();
}


VideoFrameType
Piece::video_frame_type () const
{
	DCPOMATIC_ASSERT (content->video);
	return content->video->frame_type ();
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


shared_ptr<PlayerVideo>
Piece::player_video (ContentVideo video, shared_ptr<const Film> film, dcp::Size container_size) const
{
	return std::make_shared<PlayerVideo>(
		video.image,
		content->video->crop (),
		content->video->fade (film, video.frame),
		scale_for_display(content->video->scaled_size(film->frame_size()), container_size, film->frame_size()),
		container_size,
		video.eyes,
		video.part,
		content->video->colour_conversion(),
		content->video->range(),
		content,
		video.frame,
		false
		);
}


int
Piece::resampled_audio_frame_rate (shared_ptr<const Film> film) const
{
	DCPOMATIC_ASSERT (content->audio);
	return content->audio->resampled_frame_rate (film);
}


double
Piece::audio_gain () const
{
	DCPOMATIC_ASSERT (content->audio);
	return content->audio->gain();
}


shared_ptr<Decoder>
Piece::decoder_for (shared_ptr<Content> content_) const
{
	if (content_ == content) {
		return decoder;
	}

	return {};
}


DCPTime
Piece::decoder_position () const
{
	auto t = content_time_to_dcp(content, std::max(decoder->position(), content->trim_start()));
	DCPOMATIC_ASSERT (t);
	return *t;
}


void
Piece::pass ()
{
	LOG_DEBUG_PLAYER ("Calling pass() on %1", content->path(0));
	done = decoder->pass();
}


bool
Piece::reference_dcp_audio () const
{
	auto dcp = dynamic_pointer_cast<DCPContent>(content);
	return dcp && dcp->reference_audio();
}

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


#include "atmos_decoder.h"
#include "atmos_metadata.h"
#include "audio_content.h"
#include "audio_decoder.h"
#include "content.h"
#include "dcp_content.h"
#include "dcpomatic_log.h"
#include "decoder.h"
#include "film.h"
#include "piece.h"
#include "piece_video.h"
#include "player_video.h"
#include "resampler.h"
#include "text_content.h"
#include "text_decoder.h"
#include "video_content.h"
#include "video_decoder.h"


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::pair;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;
using boost::optional;
using namespace dcpomatic;


Piece::Piece (weak_ptr<const Film> film, vector<Pair> content, FrameRateChange frc, bool fast)
	: _film (film)
	, _content (std::move(content))
	, _frc (frc)
	, _fast (fast)
{
	auto const rep = representative();
	if (rep.content->audio) {
		int const streams = rep.content->audio->streams().size();
		for (int i = 0; i < streams; ++i) {
			_audio_streams.push_back(Stream(rep.content->position(), rep.content->audio->mapping()));
		}
	}

	for (auto const& i: _content) {
		if (i.decoder->video) {
			i.decoder->video->Data.connect (boost::bind(&Piece::video, this, i.content, _1, _2, _3, _4));
		}

		if (i.decoder->audio) {
			i.decoder->audio->Data.connect (boost::bind(&Piece::audio, this, i.content, _1, _2, _3));
		}

		for (auto j: i.decoder->text) {
			j->BitmapStart.connect (boost::bind(&Piece::bitmap_start, this, i.content, j->content(), _1, _2, _3));
			j->StringStart.connect (boost::bind(&Piece::string_start, this, i.content, j->content(), _1, _2));
			j->Stop.connect (boost::bind(&Piece::stop, this, i.content, j->content(), _1));
		}

		if (i.decoder->atmos) {
			i.decoder->atmos->Data.connect (boost::bind(&Piece::atmos, this, _1, _2, _3));
		}

		i.decoder->Flush.connect (boost::bind(&Piece::flush, this));
	}
}


void
Piece::video (shared_ptr<const Content> content, shared_ptr<const ImageProxy> image, Frame frame, Eyes eyes, Part part)
{
	if (!use_video()) {
		return;
	}

	if (_frc.skip && (frame % 2) == 1) {
		return;
	}

	auto const time = content_video_to_dcp (content, frame);

	if (_ignore_video && _ignore_video->contains(time)) {
		return;
	}

	Video (PieceVideo(image, frame, time, eyes, part));
}


/** @param frame Frame position in the content, as if there were no trim */
void
Piece::audio (shared_ptr<const Content> content, AudioStreamPtr stream_ptr, shared_ptr<const AudioBuffers> audio, Frame frame)
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	/* Here we have the frame index into the content, and shortly we're going to lose that information and
	 * start thinking about frame indices into the piece.  Here's the last chance for us to apply this content's
	 * trim, so we'll take it.
	 */
	auto const start_trim = content->trim_start().frames_round(stream_ptr->frame_rate());
	auto const remove_from_start = std::max(Frame(0), start_trim - frame);
	if (remove_from_start > 0) {
		auto trimmed = make_shared<AudioBuffers>(audio);
		trimmed->trim_start (remove_from_start);
		frame += remove_from_start;
	}

	auto const end_trim = content->trim_send().frames_round(stream_ptr->frame_rate());


	auto content_streams = content->audio->streams();
	auto content_stream_iter = std::find(content_streams.begin(), content_streams.end(), stream_ptr);
	DCPOMATIC_ASSERT (content_stream_iter != content_streams.end());
	int index = std::distance(content_streams.begin(), content_stream_iter);
	DCPOMATIC_ASSERT (index >= 0 && index < static_cast<int>(_audio_streams.size()));
	auto& stream = _audio_streams[index];

	int const resampled_rate = representative().content->audio->resampled_frame_rate(film);

	auto resampler = stream.resampler;
	if (!resampler && stream_ptr->frame_rate() != resampled_rate) {
		LOG_GENERAL (
			"Creating new resampler from %1 to %2 with %3 channels",
			stream_ptr->frame_rate(),
			resampled_rate,
			stream_ptr->channels()
			);

		resampler = make_shared<Resampler>(stream_ptr->frame_rate(), resampled_rate, stream_ptr->channels());
		if (_fast) {
			resampler->set_fast ();
		}
		stream.resampler = resampler;
	}

	if (resampler) {
		auto ro = resampler->run (audio);
		if (ro->frames() == 0) {
			return;
		}
		audio = ro;
	}

	if (stream.position == 0) {
		stream.position = frame;
	}

	std::cout << "piece sends frame " << stream.position << " " << to_string(resampled_audio_to_dcp(stream.position) << "\n";
	Audio (PieceAudio(index, audio, resampled_audio_to_dcp(stream.position), stream_ptr->mapping()));
	stream.position += audio->frames();
}


void
Piece::bitmap_start (weak_ptr<const Content> content, weak_ptr<const TextContent> text, dcpomatic::ContentTime time, shared_ptr<Image> image, dcpomatic::Rect<double> area)
{
	BitmapTextStart (PieceBitmapTextStart(content, text, time, image, area));
}


void
Piece::string_start (weak_ptr<const Content> content, weak_ptr<const TextContent> text, dcpomatic::ContentTime time, list<dcp::SubtitleString> subs)
{
	StringTextStart (PieceStringTextStart(content, text, time, subs));
}


void
Piece::stop (weak_ptr<const Content> content, weak_ptr<const TextContent> text, dcpomatic::ContentTime time)
{
	TextStop (PieceTextStop(content, text, time));
}


void
Piece::atmos (shared_ptr<const dcp::AtmosFrame> data, Frame frame, AtmosMetadata metadata)
{
	Atmos (PieceAtmos(data, frame, metadata));
}


void
Piece::update_pull_to (DCPTime& pull_to) const
{
	if (done() || _audio_streams.empty()) {
		return;
	}

	for (auto const& i: _audio_streams) {
		pull_to = std::min(pull_to, i.last_push_end);
	}
}


void
Piece::set_last_push_end (int stream, DCPTime end)
{
	DCPOMATIC_ASSERT (stream >= 0 && stream < static_cast<int>(_audio_streams.size()));
	_audio_streams[stream].last_push_end = end;
}


DCPTime
Piece::content_video_to_dcp (shared_ptr<const Content> content, Frame f) const
{
	/* It might seem more logical here to convert s to a ContentTime (using the FrameRateChange)
	   then convert that ContentTime to frames at the content's rate.  However this fails for
	   situations like content at 29.9978733fps, DCP at 30fps.  The accuracy of the Time type is not
	   enough to distinguish between the two with low values of time (e.g. 3200 in Time units).

	   Instead we convert the DCPTime using the DCP video rate then account for any skip/repeat.
	*/
	auto const d = DCPTime::from_frames(f * _frc.factor(), _frc.dcp) - DCPTime(content->trim_start(), _frc);
	return d + content->position();
}


/** @param f Frame offset from start of the piece */
DCPTime
Piece::resampled_audio_to_dcp (Frame f) const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	return DCPTime::from_frames(f, film->audio_frame_rate()) + position();
}


ContentTime
Piece::dcp_to_content_time (shared_ptr<const Content> content, DCPTime t) const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	auto s = t - content->position ();
	s = min (content->length_after_trim(film), s);
	return max (ContentTime(), ContentTime(s, _frc) + content->trim_start());
}


optional<DCPTime>
Piece::content_time_to_dcp (shared_ptr<const Content> content, ContentTime t) const
{
	if (std::find_if(_content.begin(), _content.end(), [content](Pair const& p) { return p.content == content; }) == _content.end()) {
		return {};
	}

	return max (DCPTime(), DCPTime(t - content->trim_start(), _frc) + content->position());
}


bool
Piece::use_video () const
{
	for (auto const& i: _content) {
		if (i.content->video && i.content->video->use()) {
			return true;
		}
	}

	return false;
}


VideoFrameType
Piece::video_frame_type () const
{
	DCPOMATIC_ASSERT (representative().content->video);
	return representative().content->video->frame_type();
}


dcpomatic::DCPTime
Piece::position () const
{
	return _content.front().content->position();
}


dcpomatic::DCPTime
Piece::end () const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return _content.back().content->end(film);
}


shared_ptr<PlayerVideo>
Piece::player_video (PieceVideo video, dcp::Size container_size) const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	auto const rep = representative().content;

	return std::make_shared<PlayerVideo>(
		video.image,
		rep->video->crop(),
		rep->video->fade(film, video.frame),
		scale_for_display(rep->video->scaled_size(film->frame_size()), container_size, film->frame_size()),
		container_size,
		video.eyes,
		video.part,
		rep->video->colour_conversion(),
		rep->video->range(),
		/* XXX: this isn't really right, but it's just for doing the reset_metadata() tricks so it's
		 * OK that it won't work for sound-only content.
		 */
		rep,
		video.frame,
		false
		);
}


int
Piece::resampled_audio_frame_rate () const
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	DCPOMATIC_ASSERT (representative().content->audio);
	return representative().content->audio->resampled_frame_rate(film);
}


double
Piece::audio_gain () const
{
	DCPOMATIC_ASSERT (representative().content->audio);
	return representative().content->audio->gain();
}


shared_ptr<Decoder>
Piece::decoder_for (shared_ptr<Content> content) const
{
	for (auto const& i: _content) {
		if (i.content == content) {
			return i.decoder;
		}
	}

	return {};
}


void
Piece::pass ()
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	for (auto& i: _content) {
		if (i.decoder->done()) {
			continue;
		}
		LOG_DEBUG_PLAYER ("Calling pass() on %1", i.content->path(0));
		i.decoder->pass();
		return;
	}
}


bool
Piece::reference_dcp_audio () const
{
	auto dcp = dynamic_pointer_cast<DCPContent>(representative().content);
	return dcp && dcp->reference_audio();
}


void
Piece::seek (DCPTime time, bool accurate)
{
	for (auto& i: _audio_streams) {
		if (i.resampler) {
			i.resampler->flush ();
			i.resampler->reset ();
		}
		i.position = 0;
	}

	for (auto& i: _content) {
		if (time < i.content->position()) {
			/* Before; seek to the start of the content.  Even if this request is for an inaccurate seek
			   we must seek this (following) content accurately, otherwise when we come to the end of the current
			   content we may not start right at the beginning of the next, causing a gap (if the next content has
			   been trimmed to a point between keyframes, or something).
			   */
			i.decoder->seek (i.content->trim_start(), true);
		} else if (position() <= time && time < end()) {
			/* During; seek to position */
			i.decoder->seek (dcp_to_content_time(i.content, time), accurate);
		}
	}
}


optional<dcpomatic::DCPTime>
Piece::decoder_before(optional<dcpomatic::DCPTime> time)
{
	auto film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	for (auto const& i: _content) {
		if (!i.decoder->done()) {
			auto t = content_time_to_dcp(i.content, std::max(i.decoder->position(), i.content->trim_start()));
			DCPOMATIC_ASSERT (t);
			/* This is the first unfinished decoder we have, so we'll make a decision with it.
			   Given two choices at the same time, pick the one with texts so we see it before
			   the video.
			   */
			if (!time || t < *time || (t == *time && !i.decoder->text.empty())) {
				return t;
			} else {
				return {};
			}
		}
	}

	return {};
}


vector<dcpomatic::FontData>
Piece::fonts () const
{
	vector<dcpomatic::FontData> data;
	for (auto const& i: _content) {
		auto f = i.decoder->fonts();
		std::copy (f.begin(), f.end(), std::back_inserter(data));
	}
	return data;
}


DCPTimePeriod
Piece::period () const
{
	return DCPTimePeriod(position(), end());
}


void
Piece::flush ()
{
	int index = 0;
	for (auto& i: _audio_streams) {
		if (i.resampler) {
			auto ro = i.resampler->flush ();
			if (ro->frames() > 0) {
				Audio (PieceAudio(index, ro, resampled_audio_to_dcp(i.position), i.mapping));
				i.position += ro->frames();
			}
		}
		++index;
	}
}


bool
Piece::done () const
{
	for (auto const& i: _content) {
		if (!i.decoder->done()) {
			return false;
		}
	}

	return true;
}


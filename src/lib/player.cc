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
#include "audio_buffers.h"
#include "audio_content.h"
#include "audio_decoder.h"
#include "audio_processor.h"
#include "compose.hpp"
#include "config.h"
#include "dcp_content.h"
#include "dcp_decoder.h"
#include "dcpomatic_log.h"
#include "decoder.h"
#include "decoder_factory.h"
#include "ffmpeg_content.h"
#include "film.h"
#include "frame_rate_change.h"
#include "image.h"
#include "image_decoder.h"
#include "job.h"
#include "log.h"
#include "piece_video.h"
#include "player.h"
#include "player_video.h"
#include "playlist.h"
#include "ratio.h"
#include "raw_image_proxy.h"
#include "referenced_reel_asset.h"
#include "render_text.h"
#include "shuffler.h"
#include "text_content.h"
#include "text_decoder.h"
#include "timer.h"
#include "video_decoder.h"
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_closed_caption_asset.h>
#include <stdint.h>
#include <algorithm>
#include <iostream>

#include "i18n.h"


using std::copy;
using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_pair;
using std::make_shared;
using std::make_shared;
using std::map;
using std::max;
using std::min;
using std::min;
using std::pair;
using std::shared_ptr;
using std::vector;
using std::weak_ptr;
using std::unique_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


int const PlayerProperty::VIDEO_CONTAINER_SIZE = 700;
int const PlayerProperty::PLAYLIST = 701;
int const PlayerProperty::FILM_CONTAINER = 702;
int const PlayerProperty::FILM_VIDEO_FRAME_RATE = 703;
int const PlayerProperty::DCP_DECODE_REDUCTION = 704;
int const PlayerProperty::PLAYBACK_LENGTH = 705;


/** About 0.01dB */
#define AUDIO_GAIN_EPSILON 0.001


Player::Player (shared_ptr<const Film> film)
	: _film (film)
	, _suspended (0)
	, _tolerant (film->tolerant())
	, _audio_merger (_film->audio_frame_rate())
{
	construct ();
}


Player::Player (shared_ptr<const Film> film, shared_ptr<const Playlist> playlist_)
	: _film (film)
	, _playlist (playlist_)
	, _suspended (0)
	, _tolerant (film->tolerant())
	, _audio_merger (_film->audio_frame_rate())
{
	construct ();
}


void
Player::construct ()
{
	_film_changed_connection = _film->Change.connect (bind (&Player::film_change, this, _1, _2));
	/* The butler must hear about this first, so since we are proxying this through to the butler we must
	   be first.
	*/
	_playlist_change_connection = playlist()->Change.connect (bind (&Player::playlist_change, this, _1), boost::signals2::at_front);
	_playlist_content_change_connection = playlist()->ContentChange.connect (bind(&Player::playlist_content_change, this, _1, _3, _4));
	set_video_container_size (_film->frame_size ());

	film_change (ChangeType::DONE, Film::Property::AUDIO_PROCESSOR);

	setup_pieces ();
	seek (DCPTime (), true);
}


void
Player::setup_pieces ()
{
	boost::mutex::scoped_lock lm (_mutex);
	setup_pieces_unlocked ();
}


bool
have_video (shared_ptr<const Content> content)
{
	return static_cast<bool>(content->video) && content->video->use();
}


bool
have_audio (shared_ptr<const Content> content)
{
	return static_cast<bool>(content->audio);
}


vector<vector<shared_ptr<Content>>>
collect (shared_ptr<const Film> film, ContentList content)
{
	vector<shared_ptr<Content>> ungrouped;
	vector<vector<shared_ptr<Content>>> grouped;

	auto same_settings = [](shared_ptr<const Film> film, shared_ptr<const AudioContent> a, shared_ptr<const AudioContent> b) {

		auto a_streams = a->streams();
		auto b_streams = b->streams();

		if (a_streams.size() != b_streams.size()) {
			return false;
		}

		for (size_t i = 0; i < a_streams.size(); ++i) {
			auto a_stream = a_streams[i];
			auto b_stream = b_streams[i];
			if (
				!a_stream->mapping().equals(b_stream->mapping(), AUDIO_GAIN_EPSILON) ||
				a_stream->frame_rate() != b_stream->frame_rate() ||
				a_stream->channels() != b_stream->channels()) {
				return false;
			}
		}

		return (
			fabs(a->gain() - b->gain()) < AUDIO_GAIN_EPSILON &&
			a->delay() == b->delay() &&
			a->language() == b->language() &&
			a->resampled_frame_rate(film) == b->resampled_frame_rate(film) &&
			a->channel_names() == b->channel_names()
		       );
	};

	for (auto i: content) {
		if (i->video || !i->audio || !i->text.empty()) {
			ungrouped.push_back (i);
		} else {
			bool done = false;
			for (auto& g: grouped) {
				if (same_settings(film, g.front()->audio, i->audio) && i->position() == g.back()->end(film)) {
					g.push_back (i);
					done = true;
				}
			}
			if (!done) {
				grouped.push_back ({i});
			}
		}
	}

	for (auto i: ungrouped) {
		grouped.push_back({i});
	}

	return grouped;
}


void
Player::setup_pieces_unlocked ()
{
	_playback_length = _playlist ? _playlist->length(_film) : _film->length();

	auto old_pieces = _pieces;
	_pieces.clear ();

	_shuffler.reset (new Shuffler());
	_shuffler->Video.connect(bind(&Player::video, this, _1, _2));

	for (auto i: playlist()->content()) {

		if (!i->paths_valid ()) {
			continue;
		}

		if (_ignore_video && _ignore_audio && i->text.empty()) {
			/* We're only interested in text and this content has none */
			continue;
		}

		shared_ptr<Decoder> old_decoder;
		for (auto j: old_pieces) {
			auto decoder = j->decoder_for(i);
			if (decoder) {
				old_decoder = decoder;
				break;
			}
		}

		auto decoder = decoder_factory (_film, i, _tolerant, old_decoder);
		DCPOMATIC_ASSERT (decoder);

		FrameRateChange frc (_film, i);

		if (decoder->video && _ignore_video) {
			decoder->video->set_ignore (true);
		}

		if (decoder->audio && _ignore_audio) {
			decoder->audio->set_ignore (true);
		}

		if (_ignore_text) {
			for (auto i: decoder->text) {
				i->set_ignore (true);
			}
		}

		auto dcp = dynamic_pointer_cast<DCPDecoder> (decoder);
		if (dcp) {
			dcp->set_decode_referenced (_play_referenced);
			if (_play_referenced) {
				dcp->set_forced_reduction (_dcp_decode_reduction);
			}
		}

		auto piece = make_shared<Piece>(_film, i, decoder, frc, _fast);
		_pieces.push_back (piece);

		if (i->video) {
			if (i->video->frame_type() == VideoFrameType::THREE_D_LEFT || i->video->frame_type() == VideoFrameType::THREE_D_RIGHT) {
				/* We need a Shuffler to cope with 3D L/R video data arriving out of sequence */
				piece->Video.connect (bind(&Shuffler::video, _shuffler.get(), weak_ptr<Piece>(piece), _1));
			} else {
				piece->Video.connect (bind(&Player::video, this, weak_ptr<Piece>(piece), _1));
			}
		}

		if (i->audio) {
			piece->Audio.connect (bind(&Player::audio, this, weak_ptr<Piece>(piece), _1));
		}

		auto j = decoder->text.begin();

		while (j != decoder->text.end()) {
			(*j)->BitmapStart.connect (
				bind(&Player::bitmap_text_start, this, weak_ptr<Piece>(piece), i, weak_ptr<const TextContent>((*j)->content()), _1)
				);
			(*j)->PlainStart.connect (
				bind(&Player::plain_text_start, this, weak_ptr<Piece>(piece), i, weak_ptr<const TextContent>((*j)->content()), _1)
				);
			(*j)->Stop.connect (
				bind(&Player::subtitle_stop, this, weak_ptr<Piece>(piece), i, weak_ptr<const TextContent>((*j)->content()), _1)
				);

			++j;
		}

		if (decoder->atmos) {
			decoder->atmos->Data.connect (bind(&Player::atmos, this, weak_ptr<Piece>(piece), _1));
		}
	}

	for (auto i = _pieces.begin(); i != _pieces.end(); ++i) {
		if ((*i)->use_video() && (*i)->video_frame_type() != VideoFrameType::THREE_D_LEFT && (*i)->video_frame_type() != VideoFrameType::THREE_D_RIGHT) {
			/* Look for content later in the content list with in-use video that overlaps this */
			for (auto j = std::next(i); j != _pieces.end(); ++j) {
				if ((*j)->use_video()) {
					(*i)->set_ignore_video ((*j)->period().overlap((*i)->period()));
				}
			}
		}
	}

	_black = Empty (_film, playlist(), bind(&have_video, _1), _playback_length);
	_silent = Empty (_film, playlist(), bind(&have_audio, _1), _playback_length);

	_last_video_time = boost::optional<dcpomatic::DCPTime>();
	_last_video_eyes = Eyes::BOTH;
	_last_audio_time = boost::optional<dcpomatic::DCPTime>();
}


optional<DCPTime>
Player::content_time_to_dcp (shared_ptr<Content> content, ContentTime t)
{
	boost::mutex::scoped_lock lm (_mutex);

	for (auto i: _pieces) {
		auto dcp = i->content_time_to_dcp(content, t);
		if (dcp) {
			return *dcp;
		}
	}

	/* We couldn't find this content; perhaps things are being changed over */
	return {};
}


void
Player::playlist_content_change (ChangeType type, int property, bool frequent)
{
	if (property == VideoContentProperty::CROP) {
		if (type == ChangeType::DONE) {
			auto const vcs = video_container_size();
			boost::mutex::scoped_lock lm (_mutex);
			for (auto const& i: _delay) {
				i.first->reset_metadata (_film, vcs);
			}
		}
	} else {
		if (type == ChangeType::PENDING) {
			/* The player content is probably about to change, so we can't carry on
			   until that has happened and we've rebuilt our pieces.  Stop pass()
			   and seek() from working until then.
			*/
			++_suspended;
		} else if (type == ChangeType::DONE) {
			/* A change in our content has gone through.  Re-build our pieces. */
			setup_pieces ();
			--_suspended;
		} else if (type == ChangeType::CANCELLED) {
			--_suspended;
		}
	}

	Change (type, property, frequent);
}


void
Player::set_video_container_size (dcp::Size s)
{
	Change (ChangeType::PENDING, PlayerProperty::VIDEO_CONTAINER_SIZE, false);

	{
		boost::mutex::scoped_lock lm (_mutex);

		if (s == _video_container_size) {
			lm.unlock ();
			Change (ChangeType::CANCELLED, PlayerProperty::VIDEO_CONTAINER_SIZE, false);
			return;
		}

		_video_container_size = s;

		_black_image.reset (new Image (AV_PIX_FMT_RGB24, _video_container_size, true));
		_black_image->make_black ();
	}

	Change (ChangeType::DONE, PlayerProperty::VIDEO_CONTAINER_SIZE, false);
}


void
Player::playlist_change (ChangeType type)
{
	if (type == ChangeType::DONE) {
		setup_pieces ();
	}
	Change (type, PlayerProperty::PLAYLIST, false);
}


void
Player::film_change (ChangeType type, Film::Property p)
{
	/* Here we should notice Film properties that affect our output, and
	   alert listeners that our output now would be different to how it was
	   last time we were run.
	*/

	if (p == Film::Property::CONTAINER) {
		Change (type, PlayerProperty::FILM_CONTAINER, false);
	} else if (p == Film::Property::VIDEO_FRAME_RATE) {
		/* Pieces contain a FrameRateChange which contains the DCP frame rate,
		   so we need new pieces here.
		*/
		if (type == ChangeType::DONE) {
			setup_pieces ();
		}
		Change (type, PlayerProperty::FILM_VIDEO_FRAME_RATE, false);
	} else if (p == Film::Property::AUDIO_PROCESSOR) {
		if (type == ChangeType::DONE && _film->audio_processor ()) {
			boost::mutex::scoped_lock lm (_mutex);
			_audio_processor = _film->audio_processor()->clone (_film->audio_frame_rate ());
		}
	} else if (p == Film::Property::AUDIO_CHANNELS) {
		if (type == ChangeType::DONE) {
			boost::mutex::scoped_lock lm (_mutex);
			_audio_merger.clear ();
		}
	}
}


shared_ptr<PlayerVideo>
Player::black_player_video_frame (Eyes eyes) const
{
	return std::make_shared<PlayerVideo> (
		std::make_shared<const RawImageProxy>(_black_image),
		Crop(),
		optional<double>(),
		_video_container_size,
		_video_container_size,
		eyes,
		Part::WHOLE,
		PresetColourConversion::all().front().conversion,
		VideoRange::FULL,
		std::weak_ptr<Content>(),
		boost::optional<Frame>(),
		false
	);
}


vector<FontData>
Player::get_subtitle_fonts ()
{
	boost::mutex::scoped_lock lm (_mutex);

	vector<FontData> fonts;
	for (auto i: _pieces) {
		/* XXX: things may go wrong if there are duplicate font IDs
		   with different font files.
		*/
		auto f = i->fonts ();
		copy (f.begin(), f.end(), back_inserter(fonts));
	}

	return fonts;
}


/** Set this player never to produce any video data */
void
Player::set_ignore_video ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_ignore_video = true;
	setup_pieces_unlocked ();
}


void
Player::set_ignore_audio ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_ignore_audio = true;
	setup_pieces_unlocked ();
}


void
Player::set_ignore_text ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_ignore_text = true;
	setup_pieces_unlocked ();
}


/** Set the player to always burn open texts into the image regardless of the content settings */
void
Player::set_always_burn_open_subtitles ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_always_burn_open_subtitles = true;
}


/** Sets up the player to be faster, possibly at the expense of quality */
void
Player::set_fast ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_fast = true;
	setup_pieces_unlocked ();
}


void
Player::set_play_referenced ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_play_referenced = true;
	setup_pieces_unlocked ();
}


static void
maybe_add_asset (list<ReferencedReelAsset>& a, shared_ptr<dcp::ReelAsset> r, Frame reel_trim_start, Frame reel_trim_end, DCPTime from, int const ffr)
{
	DCPOMATIC_ASSERT (r);
	r->set_entry_point (r->entry_point().get_value_or(0) + reel_trim_start);
	r->set_duration (r->actual_duration() - reel_trim_start - reel_trim_end);
	if (r->actual_duration() > 0) {
		a.push_back (
			ReferencedReelAsset(r, DCPTimePeriod(from, from + DCPTime::from_frames(r->actual_duration(), ffr)))
			);
	}
}


list<ReferencedReelAsset>
Player::get_reel_assets ()
{
	/* Does not require a lock on _mutex as it's only called from DCPEncoder */

	list<ReferencedReelAsset> a;

	for (auto i: playlist()->content()) {
		auto j = dynamic_pointer_cast<DCPContent> (i);
		if (!j) {
			continue;
		}

		unique_ptr<DCPDecoder> decoder;
		try {
			decoder.reset (new DCPDecoder(_film, j, false, shared_ptr<DCPDecoder>()));
		} catch (...) {
			return a;
		}

		DCPOMATIC_ASSERT (j->video_frame_rate ());
		double const cfr = j->video_frame_rate().get();
		Frame const trim_start = j->trim_start().frames_round (cfr);
		Frame const trim_end = j->trim_end().frames_round (cfr);
		int const ffr = _film->video_frame_rate ();

		/* position in the asset from the start */
		int64_t offset_from_start = 0;
		/* position in the asset from the end */
		int64_t offset_from_end = 0;
		for (auto k: decoder->reels()) {
			/* Assume that main picture duration is the length of the reel */
			offset_from_end += k->main_picture()->actual_duration();
		}

		for (auto k: decoder->reels()) {

			/* Assume that main picture duration is the length of the reel */
			int64_t const reel_duration = k->main_picture()->actual_duration();

			/* See doc/design/trim_reels.svg */
			Frame const reel_trim_start = min(reel_duration, max(int64_t(0), trim_start - offset_from_start));
			Frame const reel_trim_end =   min(reel_duration, max(int64_t(0), reel_duration - (offset_from_end - trim_end)));

			auto const from = i->position() + DCPTime::from_frames (offset_from_start, _film->video_frame_rate());
			if (j->reference_video ()) {
				maybe_add_asset (a, k->main_picture(), reel_trim_start, reel_trim_end, from, ffr);
			}

			if (j->reference_audio ()) {
				maybe_add_asset (a, k->main_sound(), reel_trim_start, reel_trim_end, from, ffr);
			}

			if (j->reference_text (TextType::OPEN_SUBTITLE)) {
				maybe_add_asset (a, k->main_subtitle(), reel_trim_start, reel_trim_end, from, ffr);
			}

			if (j->reference_text (TextType::CLOSED_CAPTION)) {
				for (auto l: k->closed_captions()) {
					maybe_add_asset (a, l, reel_trim_start, reel_trim_end, from, ffr);
				}
			}

			offset_from_start += reel_duration;
			offset_from_end -= reel_duration;
		}
	}

	return a;
}


bool
Player::pass ()
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_suspended) {
		/* We can't pass in this state */
		LOG_DEBUG_PLAYER_NC ("Player is suspended");
		return false;
	}

	if (_playback_length == DCPTime()) {
		/* Special; just give one black frame */
		emit_video (black_player_video_frame(Eyes::BOTH), DCPTime());
		return true;
	}

	/* Find the decoder or empty which is farthest behind where we are and make it emit some data */

	shared_ptr<Piece> earliest_content;
	optional<DCPTime> earliest_time;

	for (auto i: _pieces) {
		auto time = i->decoder_before(earliest_time);
		if (time) {
			earliest_time = *time;
			earliest_content = i;
		}
	}

	bool done = false;

	enum {
		NONE,
		CONTENT,
		BLACK,
		SILENT
	} which = NONE;

	if (earliest_content) {
		which = CONTENT;
	}

	if (!_black.done() && !_ignore_video && (!earliest_time || _black.position() < *earliest_time)) {
		earliest_time = _black.position ();
		which = BLACK;
	}

	if (!_silent.done() && !_ignore_audio && (!earliest_time || _silent.position() < *earliest_time)) {
		earliest_time = _silent.position ();
		which = SILENT;
	}

	switch (which) {
	case CONTENT:
	{
		earliest_content->pass();
		if (!_play_referenced && earliest_content->reference_dcp_audio()) {
			/* We are skipping some referenced DCP audio content, so we need to update _last_audio_time
			   to `hide' the fact that no audio was emitted during the referenced DCP (though
			   we need to behave as though it was).
			*/
			_last_audio_time = earliest_content->end ();
		}
		break;
	}
	case BLACK:
		LOG_DEBUG_PLAYER ("Emit black for gap at %1", to_string(_black.position()));
		emit_video (black_player_video_frame(Eyes::BOTH), _black.position());
		_black.set_position (_black.position() + one_video_frame());
		break;
	case SILENT:
	{
		LOG_DEBUG_PLAYER ("Emit silence for gap at %1", to_string(_silent.position()));
		DCPTimePeriod period (_silent.period_at_position());
		if (_last_audio_time) {
			/* Sometimes the thing that happened last finishes fractionally before
			   or after this silence.  Bodge the start time of the silence to fix it.
			   I think this is nothing to worry about since we will just add or
			   remove a little silence at the end of some content.
			*/
			int64_t const error = labs(period.from.get() - _last_audio_time->get());
			/* Let's not worry about less than a frame at 24fps */
			int64_t const too_much_error = DCPTime::from_frames(1, 24).get();
			if (error >= too_much_error) {
				_film->log()->log(String::compose("Silence starting before or after last audio by %1", error), LogEntry::TYPE_ERROR);
			}
			DCPOMATIC_ASSERT (error < too_much_error);
			period.from = *_last_audio_time;
		}
		if (period.duration() > one_video_frame()) {
			period.to = period.from + one_video_frame();
		}
		fill_audio (period);
		_silent.set_position (period.to);
		break;
	}
	case NONE:
		done = true;
		break;
	}

	/* Emit any audio that is ready */

	/* Work out the time before which the audio is definitely all here.  This is the earliest last_push_end of one
	   of our streams, or the position of the _silent.
	*/
	auto pull_to = _playback_length;
	for (auto i: _pieces) {
		i->update_pull_to (pull_to);
	}
	if (!_silent.done() && _silent.position() < pull_to) {
		pull_to = _silent.position();
	}

	LOG_DEBUG_PLAYER("Emitting audio up to %1", to_string(pull_to));
	auto audio = _audio_merger.pull (pull_to);
	for (auto i = audio.begin(); i != audio.end(); ++i) {
		if (_last_audio_time && i->second < *_last_audio_time) {
			/* This new data comes before the last we emitted (or the last seek); discard it */
			auto cut = discard_audio (i->first, i->second, *_last_audio_time);
			if (!cut.first) {
				continue;
			}
			*i = cut;
		} else if (_last_audio_time && i->second > *_last_audio_time) {
			/* There's a gap between this data and the last we emitted; fill with silence */
			fill_audio (DCPTimePeriod (*_last_audio_time, i->second));
		}

		emit_audio (i->first, i->second);
	}

	if (done) {
		_shuffler->flush ();
		for (auto const& i: _delay) {
			do_emit_video(i.first, i.second);
		}
	}

	return done;
}


/** @return Open subtitles for the frame at the given time, converted to images */
optional<PositionImage>
Player::open_subtitles_for_frame (DCPTime time) const
{
	list<PositionImage> captions;
	int const vfr = _film->video_frame_rate();

	for (
		auto j:
		_active_texts[static_cast<int>(TextType::OPEN_SUBTITLE)].get_burnt(DCPTimePeriod(time, time + DCPTime::from_frames(1, vfr)), _always_burn_open_subtitles)
		) {

		/* Bitmap subtitles */
		for (auto i: j.bitmap) {
			if (!i.image) {
				continue;
			}

			/* i.image will already have been scaled to fit _video_container_size */
			dcp::Size scaled_size (i.rectangle.width * _video_container_size.width, i.rectangle.height * _video_container_size.height);

			captions.push_back (
				PositionImage (
					i.image,
					Position<int> (
						lrint(_video_container_size.width * i.rectangle.x),
						lrint(_video_container_size.height * i.rectangle.y)
						)
					)
				);
		}

		/* String subtitles (rendered to an image) */
		if (!j.string.empty()) {
			auto s = render_text (j.string, j.fonts, _video_container_size, time, vfr);
			copy (s.begin(), s.end(), back_inserter (captions));
		}
	}

	if (captions.empty()) {
		return {};
	}

	return merge (captions);
}


void
Player::video (weak_ptr<Piece> wp, PieceVideo video)
{
	auto piece = wp.lock ();
	if (!piece) {
		return;
	}

	if (!piece->use_video()) {
		return;
	}

	auto frc = piece->frame_rate_change();
	if (frc.skip && (video.frame % 2) == 1) {
		return;
	}

	/* Time of the first frame we will emit */
	DCPTime const time = piece->content_video_to_dcp (video.frame);
	LOG_DEBUG_PLAYER("Received video frame %1 at %2", video.frame, to_string(time));

	/* Discard if it's before the content's period or the last accurate seek.  We can't discard
	   if it's after the content's period here as in that case we still need to fill any gap between
	   `now' and the end of the content's period.
	*/
	if (time < piece->position() || (_last_video_time && time < *_last_video_time)) {
		return;
	}

	if (piece->ignore_video_at(time)) {
		return;
	}

	/* Fill gaps that we discover now that we have some video which needs to be emitted.
	   This is where we need to fill to.
	*/
	DCPTime fill_to = min (time, piece->end());

	if (_last_video_time) {
		DCPTime fill_from = max (*_last_video_time, piece->position());

		/* Fill if we have more than half a frame to do */
		if ((fill_to - fill_from) > one_video_frame() / 2) {
			auto last = _last_video.find (wp);
			if (_film->three_d()) {
				auto fill_to_eyes = video.eyes;
				if (fill_to_eyes == Eyes::BOTH) {
					fill_to_eyes = Eyes::LEFT;
				}
				if (fill_to == piece->end()) {
					/* Don't fill after the end of the content */
					fill_to_eyes = Eyes::LEFT;
				}
				auto j = fill_from;
				auto eyes = _last_video_eyes.get_value_or(Eyes::LEFT);
				if (eyes == Eyes::BOTH) {
					eyes = Eyes::LEFT;
				}
				while (j < fill_to || eyes != fill_to_eyes) {
					if (last != _last_video.end()) {
						LOG_DEBUG_PLAYER("Fill using last video at %1 in 3D mode", to_string(j));
						auto copy = last->second->shallow_copy();
						copy->set_eyes (eyes);
						emit_video (copy, j);
					} else {
						LOG_DEBUG_PLAYER("Fill using black at %1 in 3D mode", to_string(j));
						emit_video (black_player_video_frame(eyes), j);
					}
					if (eyes == Eyes::RIGHT) {
						j += one_video_frame();
					}
					eyes = increment_eyes (eyes);
				}
			} else {
				for (DCPTime j = fill_from; j < fill_to; j += one_video_frame()) {
					if (last != _last_video.end()) {
						emit_video (last->second, j);
					} else {
						emit_video (black_player_video_frame(Eyes::BOTH), j);
					}
				}
			}
		}
	}

	_last_video[wp] = piece->player_video (video, _video_container_size);

	DCPTime t = time;
	for (int i = 0; i < frc.repeat; ++i) {
		if (t < piece->end()) {
			emit_video (_last_video[wp], t);
		}
		t += one_video_frame ();
	}
}


void
Player::audio (weak_ptr<Piece> wp, PieceAudio piece_audio)
{
	DCPOMATIC_ASSERT (piece_audio.audio->frames() > 0);

	auto piece = wp.lock ();
	if (!piece) {
		return;
	}

	int const rfr = piece->resampled_audio_frame_rate ();

	/* Compute time in the DCP */
	auto time = piece->resampled_audio_to_dcp (piece_audio.frame);
	LOG_DEBUG_PLAYER("Received audio frame %1 at %2", piece_audio.frame, to_string(time));

	/* And the end of this block in the DCP */
	auto end = time + DCPTime::from_frames(piece_audio.audio->frames(), rfr);

	/* Remove anything that comes before the start or after the end of the content */
	if (time < piece->position()) {
		auto cut = discard_audio (piece_audio.audio, time, piece->position());
		if (!cut.first) {
			/* This audio is entirely discarded */
			return;
		}
		piece_audio.audio = cut.first;
		time = cut.second;
	} else if (time > piece->end()) {
		/* Discard it all */
		return;
	} else if (end > piece->end()) {
		Frame const remaining_frames = DCPTime(piece->end() - time).frames_round(rfr);
		if (remaining_frames == 0) {
			return;
		}
		piece_audio.audio = make_shared<AudioBuffers>(piece_audio.audio, remaining_frames, 0);
	}

	DCPOMATIC_ASSERT (piece_audio.audio->frames() > 0);

	/* Gain */

	if (piece->audio_gain() != 0) {
		auto gain = make_shared<AudioBuffers>(piece_audio.audio);
		gain->apply_gain (piece->audio_gain());
		piece_audio.audio = gain;
	}

	/* Remap */

	piece_audio.audio = remap (piece_audio.audio, _film->audio_channels(), piece_audio.stream->mapping());

	/* Process */

	if (_audio_processor) {
		piece_audio.audio = _audio_processor->run (piece_audio.audio, _film->audio_channels ());
	}

	/* Push */

	_audio_merger.push (piece_audio.audio, time);
	piece->set_last_push_end (piece_audio.stream, time + DCPTime::from_frames(piece_audio.audio->frames(), _film->audio_frame_rate()));
}


void
Player::bitmap_text_start (weak_ptr<Piece> wp, weak_ptr<const Content> wc, weak_ptr<const TextContent> wt, ContentBitmapText subtitle)
{
	auto piece = wp.lock ();
	auto content = wc.lock ();
	auto text = wt.lock ();
	if (!piece || !content || !text) {
		return;
	}

	/* Apply content's subtitle offsets */
	subtitle.sub.rectangle.x += text->x_offset ();
	subtitle.sub.rectangle.y += text->y_offset ();

	/* Apply a corrective translation to keep the subtitle centred after the scale that is coming up */
	subtitle.sub.rectangle.x -= subtitle.sub.rectangle.width * ((text->x_scale() - 1) / 2);
	subtitle.sub.rectangle.y -= subtitle.sub.rectangle.height * ((text->y_scale() - 1) / 2);

	/* Apply content's subtitle scale */
	subtitle.sub.rectangle.width *= text->x_scale ();
	subtitle.sub.rectangle.height *= text->y_scale ();

	PlayerText ps;
	auto image = subtitle.sub.image;

	/* We will scale the subtitle up to fit _video_container_size */
	int const width = subtitle.sub.rectangle.width * _video_container_size.width;
	int const height = subtitle.sub.rectangle.height * _video_container_size.height;
	if (width == 0 || height == 0) {
		return;
	}

	dcp::Size scaled_size (width, height);
	ps.bitmap.push_back (BitmapText(image->scale(scaled_size, dcp::YUVToRGB::REC601, image->pixel_format(), true, _fast), subtitle.sub.rectangle));
	auto from = piece->content_time_to_dcp(content, subtitle.from());
	DCPOMATIC_ASSERT (from);

	_active_texts[static_cast<int>(text->type())].add_from (wt, ps, *from);
}


void
Player::plain_text_start (weak_ptr<Piece> wp, weak_ptr<const Content> wc, weak_ptr<const TextContent> wt, ContentStringText subtitle)
{
	auto piece = wp.lock ();
	auto content = wc.lock ();
	auto text = wt.lock ();
	if (!piece || !content || !text) {
		return;
	}

	PlayerText ps;
	auto const from = piece->content_time_to_dcp(content, subtitle.from());
	DCPOMATIC_ASSERT (from);

	if (from > piece->end()) {
		return;
	}

	for (auto s: subtitle.subs) {
		s.set_h_position (s.h_position() + text->x_offset ());
		s.set_v_position (s.v_position() + text->y_offset ());
		float const xs = text->x_scale();
		float const ys = text->y_scale();
		float size = s.size();

		/* Adjust size to express the common part of the scaling;
		   e.g. if xs = ys = 0.5 we scale size by 2.
		*/
		if (xs > 1e-5 && ys > 1e-5) {
			size *= 1 / min (1 / xs, 1 / ys);
		}
		s.set_size (size);

		/* Then express aspect ratio changes */
		if (fabs (1.0 - xs / ys) > dcp::ASPECT_ADJUST_EPSILON) {
			s.set_aspect_adjust (xs / ys);
		}

		s.set_in (dcp::Time(from->seconds(), 1000));
		ps.string.push_back (StringText (s, text->outline_width()));
		ps.add_fonts (text->fonts ());
	}

	_active_texts[static_cast<int>(text->type())].add_from (wt, ps, *from);
}


void
Player::subtitle_stop (weak_ptr<Piece> wp, weak_ptr<const Content> wc, weak_ptr<const TextContent> wt, ContentTime to)
{
	auto content = wc.lock ();
	auto text = wt.lock ();
	if (!text) {
		return;
	}

	if (!_active_texts[static_cast<int>(text->type())].have(wt)) {
		return;
	}

	shared_ptr<Piece> piece = wp.lock ();
	if (!piece) {
		return;
	}

	auto const dcp_to = piece->content_time_to_dcp(content, to);
	DCPOMATIC_ASSERT (dcp_to);

	if (*dcp_to > piece->end()) {
		return;
	}

	auto from = _active_texts[static_cast<int>(text->type())].add_to(wt, *dcp_to);

	bool const always = (text->type() == TextType::OPEN_SUBTITLE && _always_burn_open_subtitles);
	if (text->use() && !always && !text->burn()) {
		Text (from.first, text->type(), text->dcp_track().get_value_or(DCPTextTrack()), DCPTimePeriod (from.second, *dcp_to));
	}
}


void
Player::seek (DCPTime time, bool accurate)
{
	boost::mutex::scoped_lock lm (_mutex);
	LOG_DEBUG_PLAYER("Seek to %1 (%2accurate)", to_string(time), accurate ? "" : "in");

	if (_suspended) {
		/* We can't seek in this state */
		return;
	}

	if (_shuffler) {
		_shuffler->clear ();
	}

	_delay.clear ();

	if (_audio_processor) {
		_audio_processor->flush ();
	}

	_audio_merger.clear ();
	for (int i = 0; i < static_cast<int>(TextType::COUNT); ++i) {
		_active_texts[i].clear ();
	}

	for (auto i: _pieces) {
		i->seek (time, accurate);
	}

	if (accurate) {
		_last_video_time = time;
		_last_video_eyes = Eyes::LEFT;
		_last_audio_time = time;
	} else {
		_last_video_time = optional<DCPTime>();
		_last_video_eyes = optional<Eyes>();
		_last_audio_time = optional<DCPTime>();
	}

	_black.set_position (time);
	_silent.set_position (time);

	_last_video.clear ();
}


void
Player::emit_video (shared_ptr<PlayerVideo> pv, DCPTime time)
{
	if (!_film->three_d()) {
		if (pv->eyes() == Eyes::LEFT) {
			/* Use left-eye images for both eyes... */
			pv->set_eyes (Eyes::BOTH);
		} else if (pv->eyes() == Eyes::RIGHT) {
			/* ...and discard the right */
			return;
		}
	}

	/* We need a delay to give a little wiggle room to ensure that relevent subtitles arrive at the
	   player before the video that requires them.
	*/
	_delay.push_back (make_pair (pv, time));

	if (pv->eyes() == Eyes::BOTH || pv->eyes() == Eyes::RIGHT) {
		_last_video_time = time + one_video_frame();
	}
	_last_video_eyes = increment_eyes (pv->eyes());

	if (_delay.size() < 3) {
		return;
	}

	auto to_do = _delay.front();
	_delay.pop_front();
	do_emit_video (to_do.first, to_do.second);
}


void
Player::do_emit_video (shared_ptr<PlayerVideo> pv, DCPTime time)
{
	if (pv->eyes() == Eyes::BOTH || pv->eyes() == Eyes::RIGHT) {
		for (int i = 0; i < static_cast<int>(TextType::COUNT); ++i) {
			_active_texts[i].clear_before (time);
		}
	}

	auto subtitles = open_subtitles_for_frame (time);
	if (subtitles) {
		pv->set_text (subtitles.get ());
	}

	Video (pv, time);
}


void
Player::emit_audio (shared_ptr<AudioBuffers> data, DCPTime time)
{
	/* Log if the assert below is about to fail */
	if (_last_audio_time && labs(time.get() - _last_audio_time->get()) > 1) {
		_film->log()->log(String::compose("Out-of-sequence emit %1 vs %2", to_string(time), to_string(*_last_audio_time)), LogEntry::TYPE_WARNING);
	}

	/* This audio must follow on from the previous, allowing for half a sample (at 48kHz) leeway */
	DCPOMATIC_ASSERT (!_last_audio_time || labs(time.get() - _last_audio_time->get()) < 2);
	Audio (data, time, _film->audio_frame_rate());
	_last_audio_time = time + DCPTime::from_frames (data->frames(), _film->audio_frame_rate());
}


void
Player::fill_audio (DCPTimePeriod period)
{
	if (period.from == period.to) {
		return;
	}

	DCPOMATIC_ASSERT (period.from < period.to);

	DCPTime t = period.from;
	while (t < period.to) {
		DCPTime block = min (DCPTime::from_seconds (0.5), period.to - t);
		Frame const samples = block.frames_round(_film->audio_frame_rate());
		if (samples) {
			auto silence = make_shared<AudioBuffers>(_film->audio_channels(), samples);
			silence->make_silent ();
			emit_audio (silence, t);
		}
		t += block;
	}
}


DCPTime
Player::one_video_frame () const
{
	return DCPTime::from_frames (1, _film->video_frame_rate ());
}


pair<shared_ptr<AudioBuffers>, DCPTime>
Player::discard_audio (shared_ptr<const AudioBuffers> audio, DCPTime time, DCPTime discard_to) const
{
	auto const discard_time = discard_to - time;
	auto const discard_frames = discard_time.frames_round(_film->audio_frame_rate());
	auto remaining_frames = audio->frames() - discard_frames;
	if (remaining_frames <= 0) {
		return make_pair(shared_ptr<AudioBuffers>(), DCPTime());
	}
	auto cut = make_shared<AudioBuffers>(audio, remaining_frames, discard_frames);
	return make_pair(cut, time + discard_time);
}


void
Player::set_dcp_decode_reduction (optional<int> reduction)
{
	Change (ChangeType::PENDING, PlayerProperty::DCP_DECODE_REDUCTION, false);

	{
		boost::mutex::scoped_lock lm (_mutex);

		if (reduction == _dcp_decode_reduction) {
			lm.unlock ();
			Change (ChangeType::CANCELLED, PlayerProperty::DCP_DECODE_REDUCTION, false);
			return;
		}

		_dcp_decode_reduction = reduction;
		setup_pieces_unlocked ();
	}

	Change (ChangeType::DONE, PlayerProperty::DCP_DECODE_REDUCTION, false);
}


shared_ptr<const Playlist>
Player::playlist () const
{
	return _playlist ? _playlist : _film->playlist();
}


void
Player::atmos (weak_ptr<Piece>, ContentAtmos data)
{
	Atmos (data.data, DCPTime::from_frames(data.frame, _film->video_frame_rate()), data.metadata);
}


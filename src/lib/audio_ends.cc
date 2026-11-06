/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "audio_buffers.h"
#include "audio_content.h"
#include "audio_ends.h"
#include "content.h"
#include "dcp_content.h"
#include "film.h"
#include "frame_rate_change.h"
#include "overlaps.h"
#include "player.h"
#include "playlist.h"
#include "sound_mxf_content.h"
#include <fmt/format.h>
#include <algorithm>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using namespace dcpomatic;


/** The most that is kept of the part of a file that a trim cuts off, at each end, in seconds.  The sync pips are a few seconds
 *  long, and a file can be much longer than the part of it that is used.
 */
static int const max_kept_seconds = 10;


/** @return the sound of @param group from @param from to @param to, on a timeline that starts with the contents, with no trims */
static shared_ptr<const AudioBuffers>
read_audio(shared_ptr<const Film> film, vector<shared_ptr<const Content>> const& group, DCPTime from, DCPTime to)
{
	auto playlist = make_shared<Playlist>();
	for (auto content: group) {
		playlist->add(film, content->untrimmed_clone());
	}

	Player player(film, playlist, false);
	player.set_ignore_video();
	player.set_ignore_text();

	int const rate = film->audio_frame_rate();
	int64_t const start = from.frames_round(rate);
	int64_t const frames = to.frames_round(rate) - start;
	auto result = make_shared<AudioBuffers>(film->audio_channels(), frames);
	result->make_silent();

	int64_t covered = start;
	player.Audio.connect([&](shared_ptr<AudioBuffers> audio, DCPTime time, int) {
		int64_t const at = time.frames_round(rate);
		int64_t const first = std::max(at, start);
		int64_t const last = std::min<int64_t>(at + audio->frames(), start + frames);
		if (last > first) {
			result->copy_from(audio.get(), last - first, first - at, first - start);
			covered = std::max(covered, last);
		}
	});

	/* A few samples short is as good as covered: the Player's end falls a little short of the contents' end when it
	 * resamples, and running it to its end would make it complain that the film is shorter than the sound.
	 */
	int64_t const slack = 32;
	player.seek(from, true);
	while (covered < start + frames - slack && !player.pass()) {}
	return result;
}


/** @return true if the film writes @param content's sound on its own, as a function of the file and the settings of this
 *  content only, and it fills one reel of the film (to within the last frame, when the sound is not a whole number of
 *  frames long)
 */
static bool
written_alone_as_a_reel(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	if (!content->audio || content->video || dynamic_pointer_cast<const SoundMXFContent>(content) || dynamic_pointer_cast<const DCPContent>(content)) {
		return false;
	}

	/* The Atmos sync signal changes from frame to frame, an audio processor has state, and every KDM of an encrypted film
	 * would list the key of a shared sound asset once for each reel that refers to it.  A delay moves sound across the
	 * reels' boundaries and a speed change makes the sound another length.
	 */
	if (film->contains_atmos_content() || film->audio_processor() || film->encrypt_sound() || content->audio->streams().size() != 1) {
		return false;
	}
	if (content->audio->delay() != 0 || FrameRateChange(film, content).change_speed()) {
		return false;
	}

	auto const frame_rate = static_cast<double>(film->video_frame_rate());
	if (content->trim_start() != content->trim_start().round(frame_rate) || content->trim_end() != content->trim_end().round(frame_rate)) {
		return false;
	}

	auto const one_frame = DCPTime::from_frames(1, film->video_frame_rate());
	auto const reels = film->reels();
	auto const reel = std::find_if(reels.begin(), reels.end(), [content, film, one_frame](DCPTimePeriod const& r) {
		return r.from == content->position() && DCPTime(content->end(film) - r.to).abs() < one_frame;
	});
	if (reel == reels.end()) {
		return false;
	}

	auto has_audio = [](shared_ptr<const Content> c) {
		return static_cast<bool>(c->audio);
	};
	return dcpomatic::overlaps(film, film->content(), has_audio, content->position(), content->end(film)).size() == 1;
}


/** @return something that is the same for two contents if and only if the film writes the same sound for them */
static string
sound_digest(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	return fmt::format(
		"{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
		content->digest(),
		content->trim_start().get(),
		content->trim_end().get(),
		content->audio->gain(),
		content->audio->fade_in().get(),
		content->audio->fade_out().get(),
		content->audio->mapping().digest(),
		film->audio_channels(),
		film->audio_frame_rate(),
		film->video_frame_rate(),
		film->interop()
		);
}


shared_ptr<const Content>
shared_audio_source(shared_ptr<const Film> film, shared_ptr<const Content> content)
{
	if (!written_alone_as_a_reel(film, content)) {
		return {};
	}

	auto const ours = sound_digest(film, content);
	shared_ptr<const Content> found;
	for (auto other: film->content()) {
		if (other == content || other->position() >= content->position() || !written_alone_as_a_reel(film, other)) {
			continue;
		}
		if (sound_digest(film, other) == ours && (!found || other->position() < found->position())) {
			found = other;
		}
	}

	return found;
}


AudioEnds
audio_ends(shared_ptr<const Film> film, vector<shared_ptr<const Content>> group, bool want_head, bool want_tail, DCPTime reel_end)
{
	AudioEnds ends;

	if (group.empty() || film->audio_frame_rate() % film->video_frame_rate() != 0) {
		return ends;
	}

	auto const rate = film->video_frame_rate();
	auto const frame_rate = static_cast<double>(rate);
	auto const first = group.front();

	for (auto content: group) {
		if (!content->audio || content->video || dynamic_pointer_cast<const SoundMXFContent>(content) || dynamic_pointer_cast<const DCPContent>(content)) {
			return ends;
		}
		/* Contents that play together are one sound only if they are cut in the same way */
		if (content->position() != first->position() || content->end(film) != first->end(film) ||
		    content->trim_start() != first->trim_start() || content->trim_end() != first->trim_end()) {
			return ends;
		}
		/* A delay moves the sound against the head and tail, a speed change makes it another length, and a trim that is not
		 * a whole number of frames leaves the joint between the head and the reel's sound off by part of a frame.
		 */
		if (content->audio->delay() != 0 || FrameRateChange(film, content).change_speed()) {
			return ends;
		}
		if (content->trim_start() != content->trim_start().round(frame_rate) || content->trim_end() != content->trim_end().round(frame_rate)) {
			return ends;
		}
	}

	auto const max_frames = static_cast<Frame>(max_kept_seconds * rate);

	if (want_head) {
		auto const trim = first->trim_start().frames_round(frame_rate);
		auto const frames = std::min(trim, max_frames);
		if (frames > 0) {
			/* The part of the head that is next to the reel's sound */
			ends.head = read_audio(film, group, DCPTime::from_frames(trim - frames, rate), DCPTime::from_frames(trim, rate));
			ends.head_frames = frames;
		}
	}

	if (want_tail) {
		auto const trim = first->trim_end().frames_round(frame_rate);
		auto const frames = std::min(trim, max_frames);
		if (frames > 0) {
			/* The part of the tail that is next to the reel's sound: the sound of the reel ends where the reel does, which is
			 * not the end of the sound when that is a part of a frame beyond.
			 */
			auto const from = first->trim_start().frames_round(frame_rate) + DCPTime(reel_end - first->position()).frames_round(rate);
			auto to = DCPTime::from_frames(from + frames, rate);
			for (auto content: group) {
				to = std::min(to, content->full_length(film));
			}
			if (to > DCPTime::from_frames(from, rate)) {
				ends.tail = read_audio(film, group, DCPTime::from_frames(from, rate), to);
			}
		}
	}

	return ends;
}

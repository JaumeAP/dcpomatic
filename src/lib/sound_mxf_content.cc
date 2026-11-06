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
#include "audio_processor.h"
#include "audio_stream.h"
#include "film.h"
#include "frame_rate_change.h"
#include "overlaps.h"
#include "job.h"
#include "sound_mxf_content.h"
#include "sound_mxf_examiner.h"
#include "util.h"
#include <asdcp/KM_log.h>
#include <dcp/exceptions.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <libxml++/libxml++.h>
#include <climits>
#include <cmath>

#include "i18n.h"


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


SoundMXFContent::SoundMXFContent(boost::filesystem::path path)
	: Content(path)
{

}


SoundMXFContent::SoundMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version)
	: Content(node, film_directory)
{
	audio = AudioContent::from_xml(this, node, version);
	audio->set_stream(
		make_shared<AudioStream>(
			node->number_child<int>("AudioFrameRate"),
			node->number_child<Frame>("AudioLength"),
			AudioMapping(node->node_child("AudioMapping"), version),
			24
			)
		);

	for (auto c: node->string_child("ChannelsWithSound")) {
		_channels_with_sound.push_back(c == '1');
	}
}


bool
SoundMXFContent::valid_mxf(boost::filesystem::path path)
{
	Kumu::DefaultLogSink().UnsetFilterFlag(Kumu::LOG_ALLOW_ALL);

	try {
		dcp::SoundAsset sound(path);
		Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);

	return false;
}


/** Give a sound MXF's stream the default mapping, leaving the channels that have no sound unmapped */
static
void
set_default_mapping(shared_ptr<AudioStream> stream, vector<bool> const& with_sound, AudioProcessor const* processor)
{
	auto mapping = stream->mapping();
	mapping.make_default(processor);
	for (size_t input = 0; input < with_sound.size(); ++input) {
		if (!with_sound[input]) {
			for (int output = 0; output < mapping.output_channels(); ++output) {
				mapping.set(input, output, 0);
			}
		}
	}
	stream->set_mapping(mapping);
}


void
SoundMXFContent::examine(shared_ptr<Job> job, bool tolerant)
{
	job->set_progress_unknown();

	Content::examine(job, tolerant);

	dcp::SoundAsset asset(path(0));
	if (asset.encrypted()) {
		throw DecodeError(_("This sound MXF is encrypted; an encrypted sound MXF cannot be added on its own."));
	}

	SoundMXFExaminer examiner(shared_from_this(), job);

	{
		boost::mutex::scoped_lock lm(_mutex);
		audio = make_shared<AudioContent>(this);
	}

	auto const with_sound = examiner.channels_with_sound();
	{
		boost::mutex::scoped_lock lm(_mutex);
		_channels_with_sound = with_sound;
	}

	auto stream = make_shared<AudioStream>(examiner.audio_frame_rate(), examiner.audio_length(), examiner.audio_channels(), 24);
	audio->set_stream(stream);
	/* There is no film to ask for an audio processor here; prepare_for_add_to_film() maps it again with the film's */
	set_default_mapping(stream, with_sound, nullptr);
}


void
SoundMXFContent::prepare_for_add_to_film(shared_ptr<const Film> film)
{
	auto with_sound = channels_with_sound();

	boost::mutex::scoped_lock lm(_mutex);

	if (audio) {
		set_default_mapping(audio->stream(), with_sound, film->audio_processor());
	}
}


string
SoundMXFContent::summary() const
{
	return fmt::format(_("{} [audio]"), path_summary());
}


string
SoundMXFContent::technical_summary() const
{
	return Content::technical_summary() + " - " + audio->technical_summary();
}


void
SoundMXFContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "SoundMXF");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);
	audio->as_xml(element);
	cxml::add_text_child(element, "AudioFrameRate", fmt::to_string(audio->stream()->frame_rate()));
	cxml::add_text_child(element, "AudioLength", fmt::to_string(audio->stream()->length()));
	audio->stream()->mapping().as_xml(cxml::add_child(element, "AudioMapping"));

	string with_sound;
	for (auto c: channels_with_sound()) {
		with_sound += c ? '1' : '0';
	}
	cxml::add_text_child(element, "ChannelsWithSound", with_sound);
}


DCPTime
SoundMXFContent::full_length(shared_ptr<const Film> film) const
{
	FrameRateChange const frc(film, shared_from_this());
	auto const stream = audio->stream();
	return DCPTime::from_frames(llrint(stream->length() / frc.speed_up()), stream->frame_rate());
}


DCPTime
SoundMXFContent::approximate_length() const
{
	return DCPTime::from_frames(audio->stream()->length(), audio->stream()->frame_rate());
}


/** The most that is kept of the part of a file that a trim cuts off, at each end, in seconds.  The sync pips are a few seconds
 *  long, and a file can be much longer than the part of it that is used.
 */
static int const max_kept_seconds = 10;


bool
SoundMXFContent::frames_align_with(shared_ptr<const Film> film) const
{
	dcp::SoundAsset const asset(path(0));
	auto const edit_rate = asset.edit_rate();
	if (edit_rate.denominator != 1 || edit_rate.numerator != film->video_frame_rate() || FrameRateChange(film, shared_from_this()).change_speed()) {
		return false;
	}

	/* A delay moves the sound against the head and tail, and a trim that is not a whole number of frames leaves the joint
	 * between the head and the reel's sound off by part of a frame.
	 */
	auto const rate = static_cast<double>(edit_rate.numerator);
	return audio->delay() == 0 && trim_start() == trim_start().round(rate) && trim_end() == trim_end().round(rate);
}


Frame
SoundMXFContent::head_frames() const
{
	dcp::SoundAsset const asset(path(0));
	auto const edit_rate = asset.edit_rate();
	auto const rate = static_cast<double>(edit_rate.numerator) / edit_rate.denominator;
	return std::min(trim_start().frames_round(rate), static_cast<Frame>(max_kept_seconds * rate));
}


shared_ptr<const AudioBuffers>
SoundMXFContent::head_audio(shared_ptr<const Film> film) const
{
	auto const frames = head_frames();
	if (frames == 0 || audio->stream()->frame_rate() != film->audio_frame_rate()) {
		return {};
	}

	/* The part of the head that is next to the reel's sound */
	dcp::SoundAsset const asset(path(0));
	auto const edit_rate = asset.edit_rate();
	auto const trim = trim_start().frames_round(static_cast<double>(edit_rate.numerator) / edit_rate.denominator);
	return frames_audio(film, trim - frames, trim);
}


shared_ptr<const AudioBuffers>
SoundMXFContent::tail_audio(shared_ptr<const Film> film) const
{
	dcp::SoundAsset const asset(path(0));
	auto const edit_rate = asset.edit_rate();
	auto const rate = static_cast<double>(edit_rate.numerator) / edit_rate.denominator;
	auto const trim = trim_end().frames_round(rate);
	auto const frames = std::min(trim, static_cast<Frame>(max_kept_seconds * rate));
	if (frames == 0 || audio->stream()->frame_rate() != film->audio_frame_rate()) {
		return {};
	}

	/* The part of the tail that is next to the reel's sound */
	auto const from = asset.intrinsic_duration() - trim;
	return frames_audio(film, from, from + frames);
}


bool
SoundMXFContent::written_alone_as_a_reel(shared_ptr<const Film> film) const
{
	/* The Atmos sync signal changes from frame to frame, an audio processor has state, and every KDM of an encrypted film would
	 * list the key of a shared sound asset once for each reel that refers to it.  A delay moves sound across the reel's
	 * boundaries and a speed change makes the sound another length.
	 */
	if (!audio || film->contains_atmos_content() || film->audio_processor() || film->encrypt_sound() || !frames_align_with(film)) {
		return false;
	}

	/* If the file goes into the DCP as it is, every reel that has it refers to the file */
	if (film->reuse_behaviour() == Film::ReuseBehaviour::COPY && !assets(film).empty()) {
		return false;
	}

	DCPTimePeriod const period(position(), end(film));
	auto const reels = film->reels();
	if (std::find(reels.begin(), reels.end(), period) == reels.end()) {
		return false;
	}

	auto has_audio = [](shared_ptr<const Content> c) {
		return static_cast<bool>(c->audio);
	};
	return dcpomatic::overlaps(film, film->content(), has_audio, period.from, period.to).size() == 1;
}


string
SoundMXFContent::sound_digest(shared_ptr<const Film> film) const
{
	return fmt::format(
		"{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}|{}",
		digest(),
		trim_start().get(),
		trim_end().get(),
		audio->gain(),
		audio->delay(),
		audio->fade_in().get(),
		audio->fade_out().get(),
		audio->stream()->mapping().digest(),
		film->audio_channels(),
		film->audio_frame_rate(),
		film->video_frame_rate(),
		film->interop()
		);
}


shared_ptr<const SoundMXFContent>
SoundMXFContent::shared_sound_source(shared_ptr<const Film> film) const
{
	if (!written_alone_as_a_reel(film)) {
		return {};
	}

	auto const ours = sound_digest(film);
	shared_ptr<const SoundMXFContent> found;
	for (auto content: film->content()) {
		auto other = std::dynamic_pointer_cast<const SoundMXFContent>(content);
		if (!other || other.get() == this || other->position() >= position() || !other->written_alone_as_a_reel(film)) {
			continue;
		}
		if (other->sound_digest(film) == ours && (!found || other->position() < found->position())) {
			found = other;
		}
	}

	return found;
}


shared_ptr<const AudioBuffers>
SoundMXFContent::frames_audio(shared_ptr<const Film> film, Frame from, Frame to) const
{
	dcp::SoundAsset asset(path(0));
	auto reader = asset.start_read();
	reader->set_check_hmac(false);

	auto const mapping = audio->stream()->mapping();
	auto result = make_shared<AudioBuffers>(film->audio_channels(), 0);
	for (auto frame = from; frame < to; ++frame) {
		result->append(remap(sound_mxf_frame_audio(reader->get_frame(frame), asset.channels()), film->audio_channels(), mapping));
	}

	/* A change is made on the whole of the file, so the gain goes on these frames too */
	result->apply_gain(audio->gain());
	return result;
}


vector<DCPAsset>
SoundMXFContent::assets(shared_ptr<const Film> film) const
{
	if (!audio || !audio->stream()) {
		return {};
	}

	dcp::SoundAsset const asset(path(0));
	auto const rate = film->video_frame_rate();

	if (asset.edit_rate().numerator != rate || asset.edit_rate().denominator != 1) {
		return {};
	}

	if ((asset.standard() == dcp::Standard::INTEROP) != film->interop()) {
		return {};
	}

	if (asset.sampling_rate() != film->audio_frame_rate() || asset.channels() != film->audio_channels()) {
		return {};
	}

	/* The film would write sound that is not the file's: the key of an encrypted sound is the film's,
	 * a processor changes the sound, and the sound of a film with Atmos has the Atmos sync signal in it.
	 */
	if (film->encrypt_sound() || film->audio_processor() || film->contains_atmos_content()) {
		return {};
	}

	if (audio->gain() != 0 || audio->delay() != 0 || audio->fade_in() != ContentTime() || audio->fade_out() != ContentTime()) {
		return {};
	}

	if (FrameRateChange(film, shared_from_this()).change_speed()) {
		return {};
	}

	/* The channels that have sound must go one to one to the DCP; the others are silent, so they may be mapped or not */
	auto const mapping = audio->stream()->mapping();
	auto const with_sound = channels_with_sound();
	for (int input = 0; input < audio->stream()->channels(); ++input) {
		auto const has_sound = static_cast<size_t>(input) >= with_sound.size() || with_sound[input];
		bool one_to_one = true;
		bool none = true;
		for (int output = 0; output < mapping.output_channels(); ++output) {
			auto const gain = mapping.get(input, output);
			if (std::abs(gain - (input == output ? 1.0F : 0.0F)) > 1e-6) {
				one_to_one = false;
			}
			if (std::abs(gain) > 1e-6) {
				none = false;
			}
		}
		if (!one_to_one && (has_sound || !none)) {
			return {};
		}
	}

	/* A trim of audio can be any number of samples, but an entry point and a duration are whole frames */
	auto const frame_rate = static_cast<double>(rate);
	if (trim_start() != trim_start().round(frame_rate) || trim_end() != trim_end().round(frame_rate)) {
		return {};
	}

	DCPTimePeriod const period(position(), end(film));
	return {
		DCPAsset(
			DCPAsset::Type::AUDIO,
			path(0),
			period,
			trim_start().frames_round(frame_rate),
			period.duration().frames_round(rate),
			boost::filesystem::path(fmt::format("pcm_{}.mxf", asset.id()))
			)
	};
}


shared_ptr<AudioBuffers>
sound_mxf_frame_audio(shared_ptr<const dcp::SoundFrame> sound_frame, int channels)
{
	auto from = sound_frame->data();
	int const frames = sound_frame->size() / (sound_frame->bits() * channels / 8);
	auto data = make_shared<AudioBuffers>(channels, frames);
	auto data_data = data->data();

	switch (sound_frame->bits()) {
	case 24:
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data_data[j][i] = static_cast<int>((from[0] << 8) | (from[1] << 16) | (from[2] << 24)) / static_cast<float>(INT_MAX - 256);
				from += 3;
			}
		}
		break;
	case 16:
		for (int i = 0; i < frames; ++i) {
			for (int j = 0; j < channels; ++j) {
				data_data[j][i] = static_cast<int>(from[0] << 16 | (from[1] << 24)) / static_cast<float>(INT_MAX - 256);
				from += 2;
			}
		}
		break;
	}

	return data;
}


void
SoundMXFContent::add_properties(shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties(film, p);
	audio->add_properties(film, p);
}

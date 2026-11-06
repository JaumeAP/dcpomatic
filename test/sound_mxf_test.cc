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


/** @file  test/sound_mxf_test.cc
 *  @brief Tests of SoundMXFContent, a PCM sound MXF added to a film on its own.
 *  @ingroup feature
 */


#include "lib/audio_analysis.h"
#include "lib/audio_buffers.h"
#include "lib/audio_content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/job_manager.h"
#include "lib/signal_manager.h"
#include "lib/sound_mxf_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/language_tag.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <cmath>


using std::dynamic_pointer_cast;
using std::string;


/** Make a DCP with a sound track and @return the path of its PCM sound MXF */
static boost::filesystem::path
make_sound_mxf(string name)
{
	auto film = new_test_film(name, { content_factory("test/data/sine_16_48_220_10.wav")[0] });
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	return find_file(film->dir(film->dcp_name()), "pcm_");
}


/** A PCM sound MXF is recognised as sound MXF content, not as something for FFmpeg */
BOOST_AUTO_TEST_CASE(sound_mxf_is_recognised_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_is_recognised_test_source");
	BOOST_CHECK(SoundMXFContent::valid_mxf(mxf));

	auto content = content_factory(mxf);
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	BOOST_CHECK(dynamic_pointer_cast<SoundMXFContent>(content[0]));

	auto film = new_test_film("sound_mxf_is_recognised_test", content);
	BOOST_REQUIRE(content[0]->audio);
	BOOST_CHECK(!content[0]->video);

	dcp::SoundAsset const asset(mxf);
	BOOST_CHECK_EQUAL(content[0]->audio->stream()->channels(), asset.channels());
	BOOST_CHECK_EQUAL(content[0]->audio->stream()->frame_rate(), asset.sampling_rate());
}


/** The sound of a DCP made from a sound MXF has the length of the file */
BOOST_AUTO_TEST_CASE(sound_mxf_makes_a_dcp_test)
{
	auto const mxf = make_sound_mxf("sound_mxf_makes_a_dcp_test_source");
	dcp::SoundAsset const source(mxf);

	auto film = new_test_film("sound_mxf_makes_a_dcp_test", content_factory(mxf));
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	BOOST_REQUIRE(reels[0]->main_sound());
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->actual_duration(), source.intrinsic_duration());
}


/** Write a 2 second sound MXF at 24fps and 48kHz with a 440Hz tone in each channel that is not in silent
 *  @param labels true to write the MCA channel labels (libdcp labels the 5.1 channels and any extra active ones)
 *  @param silent the channels that have no sound
 */
static boost::filesystem::path
write_sound_mxf(
	string name, int channels, bool labels, std::vector<int> silent, int sampling_rate = 48000, int edit_rate = 24, boost::optional<dcp::Key> key = {},
	int seconds = 2,
	bool markers = false
	)
{
	auto const path = boost::filesystem::path("build") / "test" / (name + ".mxf");
	boost::filesystem::remove(path);

	dcp::SoundAsset asset(dcp::Fraction(edit_rate, 1), sampling_rate, channels, dcp::LanguageTag("en-US"), dcp::Standard::SMPTE);
	if (key) {
		asset.set_key(*key);
	}
	auto writer = asset.start_write(
		path, {}, dcp::SoundAsset::AtmosSync::DISABLED,
		labels ? dcp::SoundAsset::MCASubDescriptors::ENABLED : dcp::SoundAsset::MCASubDescriptors::DISABLED
		);

	int const block = sampling_rate / edit_rate;
	auto make_tone = [block, sampling_rate](float level) {
		std::vector<float> tone(block);
		for (int i = 0; i < block; ++i) {
			tone[i] = level * sin(2 * M_PI * 440 * i / static_cast<double>(sampling_rate));
		}
		return tone;
	};
	auto const tone = make_tone(0.5);
	/* With markers, the first frame of the file is louder and the last quieter than the others */
	auto const first_tone = make_tone(markers ? 0.8 : 0.5);
	auto const last_tone = make_tone(markers ? 0.2 : 0.5);
	std::vector<float> quiet(block, 0.0F);

	auto data_for = [&](std::vector<float> const& tone_to_use) {
		std::vector<float const*> data(channels);
		for (int c = 0; c < channels; ++c) {
			data[c] = std::find(silent.begin(), silent.end(), c) == silent.end() ? tone_to_use.data() : quiet.data();
		}
		return data;
	};

	auto const normal_data = data_for(tone);
	auto const first_data = data_for(first_tone);
	auto const last_data = data_for(last_tone);

	int const frames = seconds * edit_rate;
	for (int i = 0; i < frames; ++i) {
		auto const& data = i == 0 ? first_data : (i == frames - 1 ? last_data : normal_data);
		writer->write(data.data(), channels, block);
	}
	writer->finalize();

	return path;
}


/** @return the sound MXF content of a new film with a file added to it */
static std::shared_ptr<SoundMXFContent>
add_sound_mxf(std::shared_ptr<Film> film, boost::filesystem::path path)
{
	auto content = content_factory(path);
	BOOST_REQUIRE_EQUAL(content.size(), 1U);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	auto sound = dynamic_pointer_cast<SoundMXFContent>(content[0]);
	BOOST_REQUIRE(sound);
	return sound;
}


/** @return true if input channel @param input is mapped to some output channel */
static bool
is_mapped(std::shared_ptr<SoundMXFContent> content, int input)
{
	auto const mapping = content->audio->stream()->mapping();
	for (int output = 0; output < mapping.output_channels(); ++output) {
		if (mapping.get(input, output) > 0) {
			return true;
		}
	}
	return false;
}


/** A labelled file: the channels that carry a label have sound; those without a label and with silence do not */
BOOST_AUTO_TEST_CASE(sound_mxf_labelled_channels_test)
{
	auto film = new_test_film("sound_mxf_labelled_channels_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_labelled_channels_test_source", 8, true, { 6, 7 }));

	BOOST_CHECK((sound->channels_with_sound() == std::vector<bool>{ true, true, true, true, true, true, false, false }));
	for (int i = 0; i < 6; ++i) {
		BOOST_CHECK_MESSAGE(is_mapped(sound, i), "channel " << i);
	}
	BOOST_CHECK(!is_mapped(sound, 6));
	BOOST_CHECK(!is_mapped(sound, 7));
	BOOST_CHECK_EQUAL(sound->audio->stream()->mapping().get(2, 2), 1.0F);
}


/** A file without labels: the channels with silence are not mapped */
BOOST_AUTO_TEST_CASE(sound_mxf_unlabelled_silence_test)
{
	auto film = new_test_film("sound_mxf_unlabelled_silence_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_unlabelled_silence_test_source", 8, false, { 3 }));

	BOOST_CHECK((sound->channels_with_sound() == std::vector<bool>{ true, true, true, false, true, true, true, true }));
	BOOST_CHECK(!is_mapped(sound, 3));
	BOOST_CHECK(is_mapped(sound, 4));
}


/** A channel with no label but with sound counts as having sound, so it is mapped */
BOOST_AUTO_TEST_CASE(sound_mxf_unlabelled_sound_test)
{
	auto film = new_test_film("sound_mxf_unlabelled_sound_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_unlabelled_sound_test_source", 8, true, { 6 }));

	BOOST_CHECK((sound->channels_with_sound() == std::vector<bool>{ true, true, true, true, true, true, false, true }));
	BOOST_CHECK(is_mapped(sound, 7));
	BOOST_CHECK(!is_mapped(sound, 6));
}


/** A file where every channel is silent is mapped one to one, so a wrong guess does not lose a channel */
BOOST_AUTO_TEST_CASE(sound_mxf_all_silent_test)
{
	auto film = new_test_film("sound_mxf_all_silent_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_all_silent_test_source", 6, false, { 0, 1, 2, 3, 4, 5 }));

	for (int i = 0; i < 6; ++i) {
		BOOST_CHECK_MESSAGE(is_mapped(sound, i), "channel " << i);
	}
}


/** The channel information comes back when the film is saved and read again */
BOOST_AUTO_TEST_CASE(sound_mxf_xml_roundtrip_test)
{
	auto film = new_test_film("sound_mxf_xml_roundtrip_test");
	auto sound = add_sound_mxf(film, write_sound_mxf("sound_mxf_xml_roundtrip_test_source", 8, false, { 3 }));
	film->write_metadata();

	auto film2 = std::make_shared<Film>(film->directory().get());
	film2->read_metadata();
	BOOST_REQUIRE_EQUAL(film2->content().size(), 1U);
	auto sound2 = dynamic_pointer_cast<SoundMXFContent>(film2->content()[0]);
	BOOST_REQUIRE(sound2);
	BOOST_CHECK(sound2->channels_with_sound() == sound->channels_with_sound());
	BOOST_CHECK(!is_mapped(sound2, 3));
	BOOST_CHECK(is_mapped(sound2, 0));
}


/** An encrypted source file is refused when it is examined, with a clear error */
BOOST_AUTO_TEST_CASE(sound_mxf_encrypted_source_is_refused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_encrypted_source_is_refused_test_source", 6, true, {}, 48000, 24, dcp::Key());

	auto film = new_test_film("sound_mxf_encrypted_source_is_refused_test");
	film->examine_and_add_content(content_factory(mxf));
	BOOST_CHECK(wait_for_jobs());
}


/** @return a new film set up so that the sound MXF at @param path can be used in it as it is */
static std::shared_ptr<Film>
film_for_public_mxf(string name, boost::filesystem::path path)
{
	dcp::SoundAsset const source(path);
	auto film = new_test_film(name);
	film->set_audio_channels(source.channels());
	film->set_video_frame_rate(source.edit_rate().numerator);
	film->set_interop(source.standard() == dcp::Standard::INTEROP);
	film->examine_and_add_content(content_factory(path));
	BOOST_REQUIRE(!wait_for_jobs());
	return film;
}


/** What the audio panel's level graph and peak read: an analysis of a sound MXF that goes into the DCP as it is */
BOOST_AUTO_TEST_CASE(sound_mxf_audio_analysis_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_audio_analysis_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	boost::signals2::connection connection;
	JobManager::instance()->analyse_audio(film, film->playlist(), false, connection, [](Job::Result) {});
	BOOST_REQUIRE(!wait_for_jobs());

	AudioAnalysis analysis(film->audio_analysis_path(film->playlist()));
	BOOST_CHECK(analysis.overall_sample_peak().first.peak > 0);
}


/** The hints analyse the real sound of a sound MXF that goes into the DCP as it is, not silence */
BOOST_AUTO_TEST_CASE(sound_mxf_hints_see_the_sound_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_hints_see_the_sound_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);

	Hints hints(film);
	hints.start();
	hints.join();
	while (signal_manager->ui_idle()) {}
	hints.rethrow();

	AudioAnalysis analysis(film->audio_analysis_path(film->playlist()));
	BOOST_CHECK(analysis.overall_sample_peak().first.peak > 0);
}


/** What the content menu's Properties shows, for a sound MXF */
BOOST_AUTO_TEST_CASE(sound_mxf_properties_test)
{
	auto film = film_for_public_mxf(
		"sound_mxf_properties_test", "test/data/audio_processor_test/pcm_908ca457-a5cd-472e-9d7d-1341917bd7d2.mxf"
		);
	BOOST_CHECK(!film->content()[0]->user_properties(film).empty());
	BOOST_CHECK(!film->content()[0]->summary().empty());
	BOOST_CHECK(!film->content()[0]->technical_summary().empty());
}


/** @return the sound reel assets of the first CPL of the DCP that @param film made, one for each reel */
static std::vector<std::shared_ptr<dcp::ReelSoundAsset>>
dcp_sound_assets(std::shared_ptr<Film> film)
{
	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	std::vector<std::shared_ptr<dcp::ReelSoundAsset>> assets;
	for (auto reel: dcp.cpls()[0]->reels()) {
		BOOST_REQUIRE(reel->main_sound());
		assets.push_back(reel->main_sound());
	}
	return assets;
}


/** Make a DCP of @param film, ignoring the notes that a film of only sound brings */
static void
make_sound_only_dcp(std::shared_ptr<Film> film)
{
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
}


/** With ReuseBehaviour::COPY a sound MXF that only a trim changes goes into the DCP as it is, and the CPL makes the trim */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_reused_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(12, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(6, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::SoundAsset(mxf).id());
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 12);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 30);
}


/** @return the largest absolute sample in channel 0 of frame @param frame of the sound MXF at @param file */
static float
frame_peak(boost::filesystem::path file, int frame)
{
	dcp::SoundAsset asset(file);
	auto reader = asset.start_read();
	reader->set_check_hmac(false);
	auto const buffers = sound_mxf_frame_audio(reader->get_frame(frame), asset.channels());
	float peak = 0;
	for (int i = 0; i < buffers->frames(); ++i) {
		peak = std::max(peak, std::abs(buffers->data(0)[i]));
	}
	return peak;
}


/** When a trimmed sound MXF has to be written again, the part that the trims cut off is kept in the sound asset and the
 *  CPL cuts it: its entry point is the head and its duration is the reel's
 */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_rewritten_keeps_head_and_tail_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_rewritten_keeps_head_and_tail_test_source", 6, true, {}, 48000, 24, {}, 3, true);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_rewritten_keeps_head_and_tail_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_gain(-3);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 72);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 63);

	/* The first frame of the file is first, the last is last, and the gain is on the head and the tail too */
	auto const file = assets[0]->asset_ref()->file();
	BOOST_REQUIRE(file);
	auto const linear = pow(10.0, -3.0 / 20.0);
	BOOST_CHECK_CLOSE(frame_peak(*file, 0), 0.8 * linear, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 5), 0.5 * linear, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 6), 0.5 * linear, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 71), 0.2 * linear, 5);
}


/** The head goes in the first reel that a sound MXF is in and the tail in the last */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_rewritten_head_and_tail_in_two_reels_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_rewritten_head_and_tail_in_two_reels_test_source", 6, true, {}, 48000, 24, {}, 3, true);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_rewritten_head_and_tail_in_two_reels_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_gain(-3);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(1) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 30);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 24);
	BOOST_CHECK_EQUAL(assets[1]->intrinsic_duration(), 42);
	BOOST_CHECK_EQUAL(assets[1]->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(assets[1]->actual_duration(), 39);

	auto const linear = pow(10.0, -3.0 / 20.0);
	auto const first = assets[0]->asset_ref()->file();
	auto const second = assets[1]->asset_ref()->file();
	BOOST_REQUIRE(first);
	BOOST_REQUIRE(second);
	BOOST_CHECK_CLOSE(frame_peak(*first, 0), 0.8 * linear, 5);
	BOOST_CHECK_CLOSE(frame_peak(*second, 41), 0.2 * linear, 5);
}


/** Only as much of the head and tail as is useful is kept, however long the part cut off is */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_rewritten_head_is_limited_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_rewritten_head_is_limited_test_source", 6, true, {}, 48000, 24, {}, 30);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_rewritten_head_is_limited_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_gain(-3);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_seconds(29));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 24);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 240);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 264);
}


/** @return the number of sound MXFs in the DCP that @param film made */
static int
sound_files_in_dcp(std::shared_ptr<Film> film)
{
	int count = 0;
	for (auto i: boost::filesystem::directory_iterator(film->dir(film->dcp_name()))) {
		if (i.path().filename().string().substr(0, 4) == "pcm_") {
			++count;
		}
	}
	return count;
}


/** @return a film with the sound MXF @param mxf twice, one after the other, each one a whole reel */
static std::shared_ptr<Film>
film_with_sound_mxf_twice(string name, boost::filesystem::path mxf)
{
	auto film = film_for_public_mxf(name, mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	auto content = content_factory(mxf);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	BOOST_REQUIRE_EQUAL(film->content().size(), 2U);
	film->content()[1]->set_position(film, film->content()[0]->end(film));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ film->content()[0]->end(film) });
	return film;
}


/** The same sound MXF as two whole reels, changed in the same way, is written once: both reels refer to the one asset */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_is_written_once_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_repeated_is_written_once_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_with_sound_mxf_twice("sound_mxf_repeated_is_written_once_test", mxf);
	for (auto content: film->content()) {
		content->audio->set_gain(-3);
	}
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), assets[1]->asset_ref().id());
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 1);
}


/** When the head and tail are kept, both reels refer to the one asset with the same entry point and duration */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_with_head_and_tail_is_written_once_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_repeated_with_head_and_tail_is_written_once_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_with_sound_mxf_twice("sound_mxf_repeated_with_head_and_tail_is_written_once_test", mxf);
	for (auto content: film->content()) {
		content->audio->set_gain(-3);
		content->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
		content->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	}
	film->content()[1]->set_position(film, film->content()[0]->end(film));
	film->set_custom_reel_boundaries({ film->content()[0]->end(film) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), assets[1]->asset_ref().id());
	for (auto asset: assets) {
		BOOST_CHECK_EQUAL(asset->entry_point().get_value_or(0), 6);
		BOOST_CHECK_EQUAL(asset->actual_duration(), 63);
		BOOST_CHECK_EQUAL(asset->intrinsic_duration(), 72);
	}
	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 1);
}


/** The same sound MXF as two whole reels, changed in different ways, is written for each reel */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_with_different_gains_is_written_twice_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_repeated_with_different_gains_is_written_twice_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_with_sound_mxf_twice("sound_mxf_repeated_with_different_gains_is_written_twice_test", mxf);
	film->content()[0]->audio->set_gain(-3);
	film->content()[1]->audio->set_gain(-6);
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK(assets[0]->asset_ref().id() != assets[1]->asset_ref().id());
	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 2);
}


/** An encrypted film does not share a sound asset between reels: every KDM would list its key twice */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_in_an_encrypted_film_is_written_twice_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_repeated_in_an_encrypted_film_is_written_twice_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_with_sound_mxf_twice("sound_mxf_repeated_in_an_encrypted_film_is_written_twice_test", mxf);
	for (auto content: film->content()) {
		content->audio->set_gain(-3);
	}
	film->set_encrypt_sound(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED
		});

	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 2);
}


/** The hints, which use a writer that makes only text, can be made for a film that shares a sound asset */
BOOST_AUTO_TEST_CASE(sound_mxf_repeated_hints_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_repeated_hints_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_with_sound_mxf_twice("sound_mxf_repeated_hints_test", mxf);
	for (auto content: film->content()) {
		content->audio->set_gain(-3);
	}

	Hints hints(film);
	hints.start();
	hints.join();
	while (signal_manager->ui_idle()) {}
	hints.rethrow();
}


/** With a delay the head and tail would not join the reel's sound, so nothing is kept */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_rewritten_with_delay_keeps_nothing_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_rewritten_with_delay_keeps_nothing_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_rewritten_with_delay_keeps_nothing_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_delay(40);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), assets[0]->actual_duration());
}


/** A trim that is not a whole number of frames keeps no head and tail */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_rewritten_with_part_frame_trim_keeps_nothing_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_rewritten_with_part_frame_trim_keeps_nothing_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_rewritten_with_part_frame_trim_keeps_nothing_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_gain(-3);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24) + dcpomatic::ContentTime::from_seconds(0.5 / 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), assets[0]->actual_duration());
}


/** Only a start trim keeps only a head */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_rewritten_keeps_head_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_rewritten_keeps_head_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_rewritten_keeps_head_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_gain(-3);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 72);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 66);
}


/** With other sound over the file nothing is kept: the asset is the reel's sound and no more */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_rewritten_with_other_sound_keeps_nothing_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_rewritten_with_other_sound_keeps_nothing_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_rewritten_with_other_sound_keeps_nothing_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_gain(-3);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	auto other = content_factory("test/data/white.wav");
	film->examine_and_add_content(other);
	BOOST_REQUIRE(!wait_for_jobs());
	other[0]->set_position(film, dcpomatic::DCPTime());
	other[0]->set_trim_end(dcpomatic::ContentTime(other[0]->full_length(film).get()) - dcpomatic::ContentTime::from_frames(63, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 63);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
}


/** A re-used sound MXF that is in two reels is in the DCP once, with the contents of the original, and the original is
 *  not shared with the DCP: changing it afterwards does not change the DCP
 */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_is_put_in_the_dcp_once_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_is_put_in_the_dcp_once_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_is_put_in_the_dcp_once_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(1) });
	make_sound_only_dcp(film);

	auto const in_dcp = film->dir(film->dcp_name()) / fmt::format("pcm_{}.mxf", dcp::SoundAsset(mxf).id());
	check_file(mxf, in_dcp);

	int count = 0;
	for (auto i: boost::filesystem::directory_iterator(film->dir(film->dcp_name()))) {
		if (i.path().filename().string().substr(0, 4) == "pcm_") {
			++count;
		}
	}
	BOOST_CHECK_EQUAL(count, 1);

	boost::system::error_code ec;
	BOOST_CHECK(!boost::filesystem::equivalent(mxf, in_dcp, ec));
}


/** A sound MXF that fills two whole reels is reused for both, each with its own entry point and duration */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_two_reels_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_two_reels_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_two_reels_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(1) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	for (auto asset: assets) {
		BOOST_CHECK_EQUAL(asset->asset_ref().id(), dcp::SoundAsset(mxf).id());
	}
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 24);
	BOOST_CHECK_EQUAL(assets[1]->entry_point().get_value_or(0), 24);
	BOOST_CHECK_EQUAL(assets[1]->actual_duration(), 24);
}


/** As above with a trim at the start: the trim is added to the second reel's entry point too */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_two_reels_with_trim_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_two_reels_with_trim_test_source", 6, true, {}, 48000, 24, {}, 3);
	auto film = film_for_public_mxf("sound_mxf_dcpasset_two_reels_with_trim_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(1) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	for (auto asset: assets) {
		BOOST_CHECK_EQUAL(asset->asset_ref().id(), dcp::SoundAsset(mxf).id());
	}
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 24);
	BOOST_CHECK_EQUAL(assets[1]->entry_point().get_value_or(0), 30);
	BOOST_CHECK_EQUAL(assets[1]->actual_duration(), 42);
}


/** A sound MXF that starts in the middle of a reel is written again: it does not fill the reels it is in */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_not_on_reel_edges_is_not_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_not_on_reel_edges_is_not_reused_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_not_on_reel_edges_is_not_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->set_position(film, dcpomatic::DCPTime::from_frames(12, 24));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(1) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
	BOOST_CHECK(assets[1]->asset_ref().id() != dcp::SoundAsset(mxf).id());
}


/** A picture MXF and a sound MXF that are cut in the same way are cut at the same frames in the CPL of each reel */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_cuts_like_the_picture_test)
{
	auto still = content_factory("test/data/flat_red.png");
	auto picture_film = new_test_film("sound_mxf_dcpasset_cuts_like_the_picture_test_picture", still);
	still[0]->video->set_length(72);
	make_and_verify_dcp(
		picture_film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});
	auto const picture_mxf = find_file(picture_film->dir(picture_film->dcp_name()), "j2c_");
	auto const sound_mxf = write_sound_mxf("sound_mxf_dcpasset_cuts_like_the_picture_test_sound", 6, true, {}, 48000, 24, {}, 3);

	auto film = film_for_public_mxf("sound_mxf_dcpasset_cuts_like_the_picture_test", sound_mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->examine_and_add_content(content_factory(picture_mxf));
	BOOST_REQUIRE(!wait_for_jobs());
	for (auto content: film->content()) {
		content->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
		content->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	}
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_frames(24, 24) });
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 2U);
	for (auto reel: reels) {
		BOOST_REQUIRE(reel->main_picture());
		BOOST_REQUIRE(reel->main_sound());
		BOOST_CHECK_EQUAL(reel->main_sound()->entry_point().get_value_or(0), reel->main_picture()->entry_point().get_value_or(0));
		BOOST_CHECK_EQUAL(reel->main_sound()->actual_duration(), reel->main_picture()->actual_duration());
	}
	BOOST_CHECK_EQUAL(reels[0]->main_sound()->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(reels[1]->main_sound()->entry_point().get_value_or(0), 30);
	BOOST_CHECK_EQUAL(reels[1]->main_sound()->actual_duration(), 39);
}


/** A sound MXF that starts a reel but ends in the middle of another one is written again */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_ends_inside_a_reel_is_not_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_ends_inside_a_reel_is_not_reused_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_ends_inside_a_reel_is_not_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	auto still = content_factory("test/data/flat_red.png");
	film->examine_and_add_content(still);
	BOOST_REQUIRE(!wait_for_jobs());
	still[0]->video->set_length(72);
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(1) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
	BOOST_CHECK(assets[1]->asset_ref().id() != dcp::SoundAsset(mxf).id());
}


/** A sound MXF with silent channels that are not mapped is still reused: the silence is in the file */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_silent_channels_are_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_silent_channels_are_reused_test_source", 8, true, { 6, 7 });
	auto film = film_for_public_mxf("sound_mxf_dcpasset_silent_channels_are_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::SoundAsset(mxf).id());
}


/** A sound MXF whose channels that have sound are not mapped one to one is written again */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_swapped_channels_are_not_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_swapped_channels_are_not_reused_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_swapped_channels_are_not_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	auto mapping = film->content()[0]->audio->stream()->mapping();
	mapping.set(0, 0, 0);
	mapping.set(0, 1, 1);
	mapping.set(1, 1, 0);
	mapping.set(1, 0, 1);
	film->content()[0]->audio->stream()->set_mapping(mapping);
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
}


/** A sound MXF with a gain is written again */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_gain_is_not_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_gain_is_not_reused_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_gain_is_not_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->content()[0]->audio->set_gain(-3);
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
}


/** A sound MXF is written again when the film does not copy */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_re_wrap_is_not_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_re_wrap_is_not_reused_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_re_wrap_is_not_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::RE_WRAP);
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
}


/** A sound MXF with other sound over it is written again */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_other_sound_is_not_reused_test)
{
	auto const mxf = write_sound_mxf("sound_mxf_dcpasset_other_sound_is_not_reused_test_source", 6, true, {});
	auto film = film_for_public_mxf("sound_mxf_dcpasset_other_sound_is_not_reused_test", mxf);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	/* The WAV is cut to 1 second and starts with the MXF, so the reel is still the MXF's period and only the other
	 * sound can stop the MXF being re-used.
	 */
	auto other = content_factory("test/data/white.wav");
	film->examine_and_add_content(other);
	BOOST_REQUIRE(!wait_for_jobs());
	other[0]->set_position(film, dcpomatic::DCPTime());
	other[0]->set_trim_end(dcpomatic::ContentTime(other[0]->full_length(film).get()) - dcpomatic::ContentTime::from_seconds(1));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::SoundAsset(mxf).id());
}


/** Two different sound MXFs with the same file name, one in each of two reels, are both in the DCP */
BOOST_AUTO_TEST_CASE(sound_mxf_dcpasset_same_file_name_test)
{
	auto copy_as_sound_mxf = [](boost::filesystem::path from, string directory) {
		auto const dir = boost::filesystem::path("build") / "test" / directory;
		boost::filesystem::remove_all(dir);
		boost::filesystem::create_directories(dir);
		boost::filesystem::copy_file(from, dir / "sound.mxf");
		return dir / "sound.mxf";
	};

	auto const first = copy_as_sound_mxf(
		write_sound_mxf("sound_mxf_dcpasset_same_file_name_test_source1", 6, true, {}), "sound_mxf_dcpasset_same_file_name_test_dir1"
		);
	auto const second = copy_as_sound_mxf(
		write_sound_mxf("sound_mxf_dcpasset_same_file_name_test_source2", 6, true, {}), "sound_mxf_dcpasset_same_file_name_test_dir2"
		);

	auto film = film_for_public_mxf("sound_mxf_dcpasset_same_file_name_test", first);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	auto content = content_factory(second);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	content[0]->set_position(film, dcpomatic::DCPTime::from_seconds(2));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(2) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::SoundAsset(first).id());
	BOOST_CHECK_EQUAL(assets[1]->asset_ref().id(), dcp::SoundAsset(second).id());
	BOOST_CHECK(assets[0]->asset_ref().id() != assets[1]->asset_ref().id());
}



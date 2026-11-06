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


/** @file  test/wav_ends_test.cc
 *  @brief Test that a trimmed WAV that is written into the DCP keeps the part its trims cut off.
 *  @ingroup feature
 */


#include "lib/audio_buffers.h"
#include "lib/audio_content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/signal_manager.h"
#include "lib/sound_mxf_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/sound_frame.h>
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <fstream>


/** Write a 16-bit 48kHz stereo WAV of @param seconds seconds with a 440Hz tone, whose first 1/24 s is louder (0.8) and
 *  whose last 1/24 s is quieter (0.2) than the rest (0.5)
 */
static boost::filesystem::path
write_marker_wav(std::string name, int seconds, int extra_samples = 0)
{
	auto const path = boost::filesystem::path("build") / "test" / (name + ".wav");
	boost::filesystem::remove(path);

	int const rate = 48000;
	int const channels = 2;
	int const block = rate / 24;
	int const frames = rate * seconds + extra_samples;

	std::vector<int16_t> samples;
	samples.reserve(frames * channels);
	for (int i = 0; i < frames; ++i) {
		double level = 0.5;
		if (i < block) {
			level = 0.8;
		} else if (i >= frames - block) {
			level = 0.2;
		}
		auto const value = static_cast<int16_t>(level * 32767 * sin(2 * M_PI * 440 * (i % block) / rate));
		for (int c = 0; c < channels; ++c) {
			samples.push_back(value);
		}
	}

	auto put32 = [](std::ofstream& out, uint32_t v) { out.write(reinterpret_cast<char const*>(&v), 4); };
	auto put16 = [](std::ofstream& out, uint16_t v) { out.write(reinterpret_cast<char const*>(&v), 2); };

	std::ofstream out(path.string(), std::ios::binary);
	uint32_t const data_bytes = samples.size() * sizeof(int16_t);
	out.write("RIFF", 4);
	put32(out, 36 + data_bytes);
	out.write("WAVEfmt ", 8);
	put32(out, 16);
	put16(out, 1);
	put16(out, channels);
	put32(out, rate);
	put32(out, rate * channels * 2);
	put16(out, channels * 2);
	put16(out, 16);
	out.write("data", 4);
	put32(out, data_bytes);
	out.write(reinterpret_cast<char const*>(samples.data()), data_bytes);
	return path;
}


static std::shared_ptr<Film>
film_for_wav(std::string name, boost::filesystem::path wav)
{
	auto film = new_test_film(name);
	film->examine_and_add_content(content_factory(wav));
	BOOST_REQUIRE(!wait_for_jobs());
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	return film;
}


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


static void
make_sound_only_dcp(std::shared_ptr<Film> film)
{
	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
}


/** @return the largest absolute sample in any channel of frame @param frame of the sound MXF at @param file */
static float
frame_peak(boost::filesystem::path file, int frame)
{
	dcp::SoundAsset asset(file);
	auto reader = asset.start_read();
	reader->set_check_hmac(false);
	auto const buffers = sound_mxf_frame_audio(reader->get_frame(frame), asset.channels());
	float peak = 0;
	for (int c = 0; c < buffers->channels(); ++c) {
		for (int i = 0; i < buffers->frames(); ++i) {
			peak = std::max(peak, std::abs(buffers->data(c)[i]));
		}
	}
	return peak;
}


/** A trimmed WAV that fills the reel keeps the part its trims cut off in the sound asset, with the gain, and the CPL
 *  starts after the head and stops before the tail
 */
BOOST_AUTO_TEST_CASE(wav_ends_are_kept_test)
{
	auto film = film_for_wav("wav_ends_are_kept_test", write_marker_wav("wav_ends_are_kept_test_source", 3));
	film->content()[0]->audio->set_gain(-3);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 72);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 63);

	auto const file = assets[0]->asset_ref()->file();
	BOOST_REQUIRE(file);
	auto const linear = pow(10.0, -3.0 / 20.0);
	BOOST_CHECK_CLOSE(frame_peak(*file, 0), 0.8 * linear, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 5), 0.5 * linear, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 6), 0.5 * linear, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 71), 0.2 * linear, 5);
}


/** The head and the tail are what the WAV has there: a fade belongs to the part that plays */
BOOST_AUTO_TEST_CASE(wav_ends_are_not_faded_test)
{
	auto film = film_for_wav("wav_ends_are_not_faded_test", write_marker_wav("wav_ends_are_not_faded_test_source", 3));
	film->content()[0]->audio->set_fade_in(dcpomatic::ContentTime::from_seconds(0.5));
	film->content()[0]->audio->set_fade_out(dcpomatic::ContentTime::from_seconds(0.5));
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	auto const file = assets[0]->asset_ref()->file();
	BOOST_REQUIRE(file);
	BOOST_CHECK_CLOSE(frame_peak(*file, 0), 0.8, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 71), 0.2, 5);
	/* The sound that plays does fade */
	BOOST_CHECK_LT(frame_peak(*file, 6), 0.2);
	BOOST_CHECK_LT(frame_peak(*file, 68), 0.2);
}


/** A WAV that is not a whole number of video frames long (a part frame of 0.75 or 0.25 here) keeps a tail that starts where
 *  the reel's sound ends
 */
BOOST_AUTO_TEST_CASE(wav_tail_of_a_part_frame_length_is_kept_test)
{
	for (auto extra: { 1500, 500 }) {
		auto const name = "wav_tail_of_a_part_frame_length_is_kept_test_" + std::to_string(extra);
		auto film = film_for_wav(name, write_marker_wav(name + "_source", 3, extra));
		film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
		make_sound_only_dcp(film);

		auto const assets = dcp_sound_assets(film);
		BOOST_REQUIRE_EQUAL(assets.size(), 1U);
		BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
		BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), assets[0]->actual_duration() + 3);
	}
}


/** With only an end trim the tail is kept and the CPL starts with the file */
BOOST_AUTO_TEST_CASE(wav_tail_only_is_kept_test)
{
	auto film = film_for_wav("wav_tail_only_is_kept_test", write_marker_wav("wav_tail_only_is_kept_test_source", 3));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 69);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 72);
	auto const file = assets[0]->asset_ref()->file();
	BOOST_REQUIRE(file);
	BOOST_CHECK_CLOSE(frame_peak(*file, 71), 0.2, 5);
}


/** The head goes in the first reel a WAV is in and the tail in the last */
BOOST_AUTO_TEST_CASE(wav_ends_in_two_reels_test)
{
	auto film = film_for_wav("wav_ends_in_two_reels_test", write_marker_wav("wav_ends_in_two_reels_test_source", 3));
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
}


/** A WAV that is resampled to the film's rate keeps its head too */
BOOST_AUTO_TEST_CASE(wav_ends_are_kept_when_resampled_test)
{
	auto film = film_for_wav("wav_ends_are_kept_when_resampled_test", "test/data/sine_16_44.1_440_10.wav");
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_seconds(2));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 48);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 192);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 240);
	auto const file = assets[0]->asset_ref()->file();
	BOOST_REQUIRE(file);
	BOOST_CHECK(frame_peak(*file, 48) > 0.001);
	/* The head is the tone, not silence (the file is a quiet tone) */
	BOOST_CHECK(frame_peak(*file, 0) > 0.001);
	BOOST_CHECK(frame_peak(*file, 47) > 0.001);
}


/** Only as much of the head as is useful is kept, however long the part cut off is */
BOOST_AUTO_TEST_CASE(wav_ends_are_limited_test)
{
	auto film = film_for_wav("wav_ends_are_limited_test", write_marker_wav("wav_ends_are_limited_test_source", 30));
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_seconds(29));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 240);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 24);
}


/** A delay moves the sound against the head, so nothing is kept */
BOOST_AUTO_TEST_CASE(wav_ends_are_not_kept_with_a_delay_test)
{
	auto film = film_for_wav("wav_ends_are_not_kept_with_a_delay_test", write_marker_wav("wav_ends_are_not_kept_with_a_delay_test_source", 3));
	film->content()[0]->audio->set_delay(10);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), assets[0]->actual_duration());
}


/** WAVs that play together with the same position, length and trims are one sound: their heads and tails are mixed and kept */
BOOST_AUTO_TEST_CASE(wav_ends_of_wavs_that_play_together_are_kept_test)
{
	auto film = film_for_wav("wav_ends_of_wavs_that_play_together_are_kept_test", write_marker_wav("wav_ends_of_wavs_that_play_together_are_kept_test_first", 3));
	auto second = content_factory(write_marker_wav("wav_ends_of_wavs_that_play_together_are_kept_test_second", 3));
	film->examine_and_add_content(second);
	BOOST_REQUIRE(!wait_for_jobs());
	second[0]->set_position(film, dcpomatic::DCPTime());
	for (auto content: film->content()) {
		/* Two of the same level add up to the level of one without the gain */
		content->audio->set_gain(-6.0206);
		content->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
		content->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	}
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), 72);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 63);

	auto const file = assets[0]->asset_ref()->file();
	BOOST_REQUIRE(file);
	BOOST_CHECK_CLOSE(frame_peak(*file, 0), 0.8, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 6), 0.5, 5);
	BOOST_CHECK_CLOSE(frame_peak(*file, 71), 0.2, 5);
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


/** @return a film with the WAV @param wav twice, one after the other, each one a whole reel */
static std::shared_ptr<Film>
film_with_wav_twice(std::string name, boost::filesystem::path wav)
{
	auto film = film_for_wav(name, wav);
	film->examine_and_add_content(content_factory(wav));
	BOOST_REQUIRE(!wait_for_jobs());
	BOOST_REQUIRE_EQUAL(film->content().size(), 2U);
	film->content()[1]->set_position(film, film->content()[0]->end(film));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ film->content()[0]->end(film) });
	return film;
}


/** The same WAV as two whole reels, changed in the same way, is written once: both reels refer to the one asset */
BOOST_AUTO_TEST_CASE(wav_repeated_is_written_once_test)
{
	auto film = film_with_wav_twice("wav_repeated_is_written_once_test", write_marker_wav("wav_repeated_is_written_once_test_source", 3));
	for (auto content: film->content()) {
		content->audio->set_gain(-3);
	}
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), assets[1]->asset_ref().id());
	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 1);
}


/** When the head and tail are kept, both reels refer to the one asset with the same entry point and duration */
BOOST_AUTO_TEST_CASE(wav_repeated_with_head_and_tail_is_written_once_test)
{
	auto film = film_with_wav_twice("wav_repeated_with_head_and_tail_is_written_once_test", write_marker_wav("wav_repeated_with_head_and_tail_is_written_once_test_source", 3));
	for (auto content: film->content()) {
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


/** The same WAV as two whole reels, changed in different ways, is written for each reel */
BOOST_AUTO_TEST_CASE(wav_repeated_with_different_gains_is_written_twice_test)
{
	auto film = film_with_wav_twice("wav_repeated_with_different_gains_is_written_twice_test", write_marker_wav("wav_repeated_with_different_gains_is_written_twice_test_source", 3));
	film->content()[0]->audio->set_gain(-3);
	film->content()[1]->audio->set_gain(-6);
	make_sound_only_dcp(film);

	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 2);
}


/** An encrypted film does not share a sound asset between reels: every KDM would list its key twice */
BOOST_AUTO_TEST_CASE(wav_repeated_in_an_encrypted_film_is_written_twice_test)
{
	auto film = film_with_wav_twice("wav_repeated_in_an_encrypted_film_is_written_twice_test", write_marker_wav("wav_repeated_in_an_encrypted_film_is_written_twice_test_source", 3));
	film->set_encrypt_sound(true);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED
		});

	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 2);
}


/** Two WAVs that end inside a frame (71.25 frames here) fill reels of the same length, so they are written once */
BOOST_AUTO_TEST_CASE(wav_repeated_ending_inside_a_frame_is_written_once_test)
{
	auto film = film_with_wav_twice(
		"wav_repeated_ending_inside_a_frame_is_written_once_test",
		write_marker_wav("wav_repeated_ending_inside_a_frame_is_written_once_test_source", 3, -1500)
		);
	film->content()[1]->set_position(film, dcpomatic::DCPTime::from_frames(72, 24));
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_frames(72, 24) });
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 72);
	BOOST_CHECK_EQUAL(assets[1]->actual_duration(), 72);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), assets[1]->asset_ref().id());
	BOOST_CHECK_EQUAL(sound_files_in_dcp(film), 1);
}


/** The hints, which use a writer that makes only text, can be made for a film that shares a sound asset */
BOOST_AUTO_TEST_CASE(wav_repeated_hints_test)
{
	auto film = film_with_wav_twice("wav_repeated_hints_test", write_marker_wav("wav_repeated_hints_test_source", 3));

	Hints hints(film);
	hints.start();
	hints.join();
	while (signal_manager->ui_idle()) {}
	hints.rethrow();
}


/** Other sound over the WAV means the reel's sound is not the WAV's alone, so nothing is kept */
BOOST_AUTO_TEST_CASE(wav_ends_are_not_kept_with_other_sound_over_test)
{
	auto film = film_for_wav("wav_ends_are_not_kept_with_other_sound_over_test", write_marker_wav("wav_ends_are_not_kept_with_other_sound_over_test_source", 3));
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	auto other = content_factory(write_marker_wav("wav_ends_are_not_kept_with_other_sound_over_test_other", 3));
	film->examine_and_add_content(other);
	BOOST_REQUIRE(!wait_for_jobs());
	other[0]->set_position(film, dcpomatic::DCPTime());
	make_sound_only_dcp(film);

	auto const assets = dcp_sound_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(assets[0]->intrinsic_duration(), assets[0]->actual_duration());
}

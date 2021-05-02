/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/sound_asset.h>
#include <boost/test/unit_test.hpp>
#include <sndfile.h>


static
void
check_asset_against_wav (std::shared_ptr<const Film> film, boost::filesystem::path ref_path)
{
	dcp::DCP dcp (film->dir(film->dcp_name()));
	dcp.read ();
	BOOST_REQUIRE_EQUAL (dcp.cpls().size(), 1U);
	auto cpl = dcp.cpls()[0];
	BOOST_REQUIRE_EQUAL (cpl->reels().size(), 1U);
	auto reel = cpl->reels()[0];
	BOOST_REQUIRE (reel->main_sound());
	auto asset = reel->main_sound()->asset();
	BOOST_REQUIRE (asset);
	auto reader = asset->start_read ();

	SF_INFO info;
	info.format = 0;
	auto ref = sf_open (ref_path.c_str(), SFM_READ, &info);
	BOOST_REQUIRE (ref);

	BOOST_REQUIRE_EQUAL (info.channels, 1U);

	auto constexpr block_size = 2000;
	std::vector<short int> buffer (block_size);

	for (int i = 0; i < asset->intrinsic_duration(); ++i) {
		auto frame = reader->get_frame(i);
		sf_count_t this_time = std::min(info.frames, static_cast<sf_count_t>(block_size));
		sf_readf_short (ref, buffer.data(), this_time);
		for (int j = 0; j < this_time; ++j) {
			auto s = frame->get(2, j);
			if (s > (1 << 23)) {
				s -= (1 << 24);
			}
			auto diff = std::abs(s / 256 - buffer[j]);
			BOOST_REQUIRE_MESSAGE (diff <= 1, "failed on asset frame " << i << " sample " << j << " reference " << buffer[j] << " cf " << s);
		}
		info.frames -= this_time;
	}

	sf_close (ref);
}


/** Make a DCP with a sine wave and make sure it comes out the same as it went in */
BOOST_AUTO_TEST_CASE (test_audio_passthrough)
{
	boost::filesystem::path const ref = "test/data/sine_440.wav";
	auto content = content_factory(ref).front();
	auto film = new_test_film2 ("test_audio_passthrough", { content });
	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	check_asset_against_wav (film, ref);
}


static
int
split_wav (boost::filesystem::path source_path, std::vector<boost::filesystem::path> part_paths, int offset)
{
	SF_INFO info;
	info.format = 0;
	auto source = sf_open (source_path.c_str(), SFM_READ, &info);
	BOOST_REQUIRE (source);

	BOOST_REQUIRE (offset < info.frames);

	BOOST_REQUIRE_EQUAL (part_paths.size(), 2U);

	std::vector<SNDFILE*> parts;
	for (auto const& i: part_paths) {
		SF_INFO part_info;
		part_info.samplerate = info.samplerate;
		part_info.channels = info.channels;
		part_info.format = info.format;
		auto part = sf_open (i.c_str(), SFM_WRITE, &part_info);
		BOOST_REQUIRE (part);
		parts.push_back (part);
	}

	std::vector<short int> buffer (offset);
	sf_readf_short (source, buffer.data(), offset);
	sf_writef_short (parts[0], buffer.data(), offset);
	buffer.resize (info.frames - offset);
	sf_readf_short (source, buffer.data(), info.frames - offset);
	sf_writef_short (parts[1], buffer.data(), info.frames - offset);

	for (auto const& i: parts) {
		sf_close (i);
	}

	sf_close (source);

	return info.samplerate;
}


static
void
test_with_split (std::string name, boost::filesystem::path source, int offset, bool resample)
{
	boost::filesystem::path parts[2] = {
		"build/test/" + name + "_part1.wav",
		"build/test/" + name + "_part2.wav"
	};

	auto sample_rate = split_wav (source, { parts[0], parts[1] }, offset);

	std::shared_ptr<Content> content[2] = {
		content_factory(parts[0]).front(),
		content_factory(parts[1]).front()
		};

	auto constexpr resample_rate = 23.976;

	auto film = new_test_film2 (name, { content[0], content[1] });
	content[1]->set_position (film, dcpomatic::DCPTime::from_frames(offset, resample ? (sample_rate * resample_rate / 24) : sample_rate));

	if (resample) {
		content[0]->set_video_frame_rate (resample_rate);
		content[1]->set_video_frame_rate (resample_rate);
	}

	make_and_verify_dcp (film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	check_asset_against_wav (film, source);
}


BOOST_AUTO_TEST_CASE (test_audio_passthrough_with_split_36000)
{
	test_with_split ("test_audio_passthrough_with_split_36000", "test/data/sine_440.wav", 36000, false);
}


BOOST_AUTO_TEST_CASE (test_audio_passthrough_with_split_24000)
{
	test_with_split ("test_audio_passthrough_with_split_24000", "test/data/sine_440.wav", 24000, false);
}


BOOST_AUTO_TEST_CASE (test_audio_passthrough_with_split_23999)
{
	test_with_split ("test_audio_passthrough_with_split_23999", "test/data/sine_440.wav", 23999, false);
}


BOOST_AUTO_TEST_CASE (test_audio_passthrough_with_split_36000_resampled)
{
	test_with_split ("test_audio_passthrough_with_split_36000_resampled", "test/data/sine_440.wav", 36000, true);
}


BOOST_AUTO_TEST_CASE (test_audio_passthrough_with_split_24000_resampled)
{
	test_with_split ("test_audio_passthrough_with_split_24000_resampled", "test/data/sine_440.wav", 24000, true);
}

BOOST_AUTO_TEST_CASE (test_audio_passthrough_with_split_23999_resampled)
{
	test_with_split ("test_audio_passthrough_with_split_23999_resampled", "test/data/sine_440.wav", 23999, true);
}


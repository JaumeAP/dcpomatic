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


/** @file  test/lua_test.cc
 *  @brief Tests of the Lua scripting state and its unit helpers.
 *  @ingroup feature
 */


#include "lib/audio_content.h"
#include "lib/content.h"
#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/film.h"
#include "lib/filter.h"
#include "lib/font.h"
/* lib/job_manager.h, which has no include guard, comes from lua_test_helpers.h */
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "lua/lua_state.h"
#include "lua/property_table.h"
#include "lua_test_helpers.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/language_tag.h>
#include <dcp/reel.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/scope_guard.h>
#include <dcp/sound_asset.h>
#include <dcp/util.h>
#include <fmt/ranges.h>
#include <boost/regex.hpp>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <clocale>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sstream>
#include <string>
#include <vector>


using std::make_shared;
using std::string;
using std::vector;
using boost::optional;


/** A script can be run without error */
BOOST_AUTO_TEST_CASE(lua_runs_a_string)
{
	LuaState state;
	BOOST_CHECK_NO_THROW(state.run_string("x = 1 + 1"));
}


/** A syntax error is reported as a LuaError naming the chunk and line */
BOOST_AUTO_TEST_CASE(lua_reports_syntax_error)
{
	LuaState state;
	try {
		state.run_string("x = = 1");
		BOOST_ERROR("no exception thrown");
	} catch (LuaError& e) {
		BOOST_CHECK_MESSAGE(string(e.what()).find("script:1") != string::npos, e.what());
	}
}


/** Numbers printed by a script use '.' whatever the decimal separator of the environment's locale */
BOOST_AUTO_TEST_CASE(lua_numbers_ignore_locale)
{
	string const old_locale = setlocale(LC_NUMERIC, nullptr);
	dcp::ScopeGuard sg = [old_locale]() { setlocale(LC_NUMERIC, old_locale.c_str()); };

	auto found = false;
	for (auto name: { "ca_ES.UTF-8", "de_DE.UTF-8", "fr_FR.UTF-8" }) {
		if (setlocale(LC_NUMERIC, name)) {
			found = true;
			break;
		}
	}

	if (!found) {
		BOOST_TEST_MESSAGE("lua_numbers_ignore_locale skipped: no locale with a decimal comma is installed");
		return;
	}

	LuaState state;
	state.run_string("a = tostring(-3.01)\nb = string.format('%.2f', 1.5)");
	BOOST_CHECK_EQUAL(get_global(state, "a"), "-3.01");
	BOOST_CHECK_EQUAL(get_global(state, "b"), "1.50");
}


/** The dcpomatic table converts seconds, frames and timecodes to ticks (96000 per second) */
BOOST_AUTO_TEST_CASE(lua_units)
{
	LuaState state;
	BOOST_CHECK_NO_THROW(state.run_string(
		"assert(dcpomatic.seconds(8) == 768000)\n"
		"assert(math.type(dcpomatic.seconds(8)) == 'integer')\n"
		"assert(dcpomatic.frames(8, 24) == 32000)\n"
		"assert(dcpomatic.timecode('00:00:08:00', 24) == 768000)\n"
		"assert(not pcall(dcpomatic.seconds, 0.00001))\n"
		"assert(dcpomatic.frames(1, 24000 / 1001) == 4004)\n"
		"assert(dcpomatic.timecode('00:00:01:01', 24000 / 1001) == 96000 + 4004)\n"
		"assert(not pcall(dcpomatic.frames, 1, 23.976))\n"
		"assert(not pcall(dcpomatic.timecode, '00:00:00:01', 23.976))\n"
		"assert(not pcall(dcpomatic.frames, 1, 0 / 0))\n"
		));
}


/** A property set from Lua and saved is in the metadata that the library reads back */
BOOST_AUTO_TEST_CASE(lua_film_set_and_get)
{
	auto film = new_test_film("lua_film_set_and_get");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:set('audio_channels', 12)\n"
		"assert(film:get('audio_channels') == 12)\n"
		"film:save()\n"
		);

	auto check = make_shared<Film>(*film->directory());
	check->read_metadata();
	BOOST_CHECK_EQUAL(check->audio_channels(), 12);
}


/** A misspelt property name raises an error that names the closest real one */
BOOST_AUTO_TEST_CASE(lua_film_unknown_property)
{
	auto film = new_test_film("lua_film_unknown_property");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	auto const error = error_from(state, "dcpomatic.open_film(film_dir):set('audio_chanels', 12)");
	BOOST_CHECK_MESSAGE(error.find("unknown property 'audio_chanels'") != string::npos, error);
	BOOST_CHECK_MESSAGE(error.find("did you mean 'audio_channels'") != string::npos, error);
}


/** A value of the wrong type raises an error that names the property */
BOOST_AUTO_TEST_CASE(lua_film_wrong_type)
{
	auto film = new_test_film("lua_film_wrong_type");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	auto const error = error_from(state, "dcpomatic.open_film(film_dir):set('audio_channels', 'twelve')");
	BOOST_CHECK_MESSAGE(error.find("property 'audio_channels': expected integer") != string::npos, error);
}


/** A script that raises an error before film:save() leaves metadata.xml as it was */
BOOST_AUTO_TEST_CASE(lua_error_keeps_metadata)
{
	auto film = new_test_film("lua_error_keeps_metadata");
	auto const metadata = film->file("metadata.xml");
	auto const before = dcp::file_to_string(metadata);

	{
		LuaState state;
		set_global(state, "film_dir", film->directory()->string());
		auto const error = error_from(state,
			"local film = dcpomatic.open_film(film_dir)\n"
			"film:set('name', 'Changed')\n"
			"error('boom')\n"
			);
		BOOST_CHECK_MESSAGE(error.find("boom") != string::npos, error);
	}

	BOOST_CHECK(dcp::file_to_string(metadata) == before);
}


/** Content whose path has spaces and non-ASCII characters can be added */
BOOST_AUTO_TEST_CASE(lua_add_content_path_with_spaces)
{
	auto film = new_test_film("lua_add_content_path_with_spaces");

	auto const dir = test_film_dir("lua_add_content_path_with_spaces_content") / "ünï cöde dir";
	boost::filesystem::remove_all(dir.parent_path());
	boost::filesystem::create_directories(dir);
	boost::filesystem::copy_file("test/data/flat_red.png", dir / "flat_red.png");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	/* Content paths are made canonical when the content is created */
	set_global(state, "content_path", (boost::filesystem::canonical(dir) / "flat_red.png").string());
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local content = film:add_content(content_path)\n"
		"assert(content:path() == content_path, content:path())\n"
		"assert(#film:content() == 1)\n"
		"assert(film:content()[1]:path() == content_path)\n"
		);
}


/** describe() gives the trim of each piece of content */
BOOST_AUTO_TEST_CASE(lua_describe_lists_trim)
{
	auto film = new_test_film("lua_describe_lists_trim", content_factory("test/data/flat_red.png"));
	film->write_metadata();

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	check_runs(state, "description = dcpomatic.open_film(film_dir):describe()");
	auto const description = get_global(state, "description");
	BOOST_CHECK_MESSAGE(description.find("\ntrim_end: 0\n") != string::npos, description);
	BOOST_CHECK_MESSAGE(description.find("audio_channels: 6\n") != string::npos, description);
}


/** describe() gives the index of each piece of content, the settings of its video, audio and text, and its mapping */
BOOST_AUTO_TEST_CASE(lua_describe_lists_parts)
{
	LuaState state;
	film_and_wav(state, "lua_describe_lists_parts", 1, 1);
	set_global(state, "picture", boost::filesystem::canonical("test/data/flat_red.png").string());
	set_global(state, "subtitles", boost::filesystem::canonical("test/data/15s.srt").string());
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		"film:add_content(subtitles)\n"
		"description = film:describe()\n"
		);
	auto const description = get_global(state, "description");
	BOOST_CHECK_MESSAGE(description.find("index: 2\n") != string::npos, description);
	BOOST_CHECK_MESSAGE(description.find("video.left_crop: 0\n") != string::npos, description);
	BOOST_CHECK_MESSAGE(description.find("audio.gain: 0\n") != string::npos, description);
	/* A mono file goes to the centre */
	BOOST_CHECK_MESSAGE(description.find("audio.mapping[1][3]: 1\n") != string::npos, description);
	BOOST_CHECK_MESSAGE(description.find("text.x_offset: 0\n") != string::npos, description);
}


/** Trimming 8 seconds from the end of 20 seconds of sound gives a DCP with 12 seconds of sound */
BOOST_AUTO_TEST_CASE(lua_trim_end_shortens_dcp_audio)
{
	LuaState state;
	auto const dir = film_and_wav(state, "lua_trim_end_shortens_dcp_audio", 1, 20);
	/* A DCP needs a picture; the still is shorter than the sound, so the sound sets the length */
	set_global(state, "picture", boost::filesystem::canonical("test/data/flat_red.png").string());
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"local sound = film:add_content(wav)\n"
		"sound:set('trim_end', dcpomatic.seconds(8))\n"
		/* 6 channels and no audio language give hints */
		"film:make_dcp{ ignore_hints = true }\n"
		/* make_dcp names a film that has no name, which changes where the DCP is */
		"film:save()\n"
		);

	auto film = make_shared<Film>(dir);
	film->read_metadata();
	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	auto const reels = dcp.cpls()[0]->reels();
	BOOST_REQUIRE_EQUAL(reels.size(), 1U);
	auto const sound = reels[0]->main_sound();
	BOOST_REQUIRE(sound);
	auto const edit_rate = sound->asset()->edit_rate();
	auto const samples = sound->asset()->intrinsic_duration() * sound->asset()->sampling_rate() * edit_rate.denominator / edit_rate.numerator;
	BOOST_CHECK_EQUAL(samples, 12 * 48000);
}


/** A trim that is not a whole number of ticks raises an error that names the property */
BOOST_AUTO_TEST_CASE(lua_trim_wrong_type)
{
	LuaState state;
	film_and_wav(state, "lua_trim_wrong_type", 1, 1);
	auto const error = error_from(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(wav):set('trim_end', 0.5)\n"
		);
	BOOST_CHECK_MESSAGE(error.find("property 'trim_end': expected integer ticks") != string::npos, error);
}


/** A change to the mapping made with 1-based channel numbers is read back, saved and cleared */
BOOST_AUTO_TEST_CASE(lua_mapping_roundtrip)
{
	LuaState state;
	auto const dir = film_and_wav(state, "lua_mapping_roundtrip", 8, 1);
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local c = film:add_content(wav)\n"
		"c:map(5, 11, 1)\n"
		"assert(c:mapping()[5][11] == 1)\n"
		"assert(not pcall(c.map, c, 9, 1, 1))\n"
		"assert(not pcall(c.map, c, 1, 0, 1))\n"
		"film:save()\n"
		"local cleared = dcpomatic.open_film(film_dir):content()[1]\n"
		"cleared:clear_mapping()\n"
		"assert(cleared:mapping()[5][11] == 0)\n"
		);

	auto film = make_shared<Film>(dir);
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_CHECK_EQUAL(film->content()[0]->audio->mapping().get(4, 10), 1);
}


/** A Content whose content has been removed from its film raises an error when used */
BOOST_AUTO_TEST_CASE(lua_removed_content_handle)
{
	LuaState state;
	film_and_wav(state, "lua_removed_content_handle", 1, 1);
	auto const error = error_from(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local c = film:add_content(wav)\n"
		"film:remove_content(c)\n"
		"c:get('trim_end')\n"
		);
	BOOST_CHECK_MESSAGE(error.find("content was removed from the film") != string::npos, error);
}


/** Audio gain set from Lua is the gain of the AudioContent */
BOOST_AUTO_TEST_CASE(lua_audio_gain)
{
	LuaState state;
	auto const dir = film_and_wav(state, "lua_audio_gain", 1, 1);
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local c = film:add_content(wav)\n"
		"c:set('audio.gain', 3)\n"
		"assert(c:get('audio.gain') == 3)\n"
		"film:save()\n"
		);

	auto film = make_shared<Film>(dir);
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_CHECK_EQUAL(film->content()[0]->audio->gain(), 3);
}


/** Trimming the first of two sequenced pieces of content moves the second one up, before anything else is called */
BOOST_AUTO_TEST_CASE(lua_trim_resequences)
{
	auto film = new_test_film("lua_trim_resequences");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	set_global(state, "picture", boost::filesystem::canonical("test/data/flat_red.png").string());
	/* Only video and text are sequenced; a still lasts 10 seconds */
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local first = film:add_content(picture)\n"
		"film:add_content(picture)\n"
		"assert(film:content()[2]:get('position') == dcpomatic.seconds(10))\n"
		"first:set('trim_end', dcpomatic.seconds(4))\n"
		"film:save()\n"
		);

	auto check = make_shared<Film>(*film->directory());
	check->read_metadata();
	BOOST_REQUIRE_EQUAL(check->content().size(), 2U);
	BOOST_CHECK_EQUAL(check->content()[1]->position().get(), dcpomatic::DCPTime::from_seconds(6).get());
}


/** With subtitles and closed captions in one piece of content, each is set and read by its original type */
BOOST_AUTO_TEST_CASE(lua_text_parts_by_type)
{
	/* film:add_content uses content_factory, which does not make DCPContent */
	auto film = new_test_film("lua_text_parts_by_type", { make_shared<DCPContent>(TestPaths::private_data() / "two_text_tracks") });
	film->write_metadata();

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:content()[1]\n"
		"c:set('text.y_offset', 0.25, 'closed-caption')\n"
		"assert(c:get('text.y_offset', 'closed-caption') == 0.25)\n"
		"assert(c:get('text.y_offset', 'open-subtitle') == 0)\n"
		"description = film:describe()\n"
		);

	auto const ambiguous = error_from(state, "c:get('text.y_offset')");
	BOOST_CHECK_MESSAGE(ambiguous.find("property 'text.y_offset': the content has several texts") != string::npos, ambiguous);
	auto const missing = error_from(state, "c:get('text.y_offset', 'open-caption')");
	BOOST_CHECK_MESSAGE(missing.find("property 'text.y_offset': the content has no open-caption text") != string::npos, missing);

	auto const description = get_global(state, "description");
	BOOST_CHECK_MESSAGE(description.find("text part: open-subtitle\n") != string::npos, description);
	BOOST_CHECK_MESSAGE(description.find("text part: closed-caption\n") != string::npos, description);
	BOOST_CHECK_MESSAGE(description.find("text.y_offset: 0.25\n") != string::npos, description);
}


/** A video fade is given in ticks, like every other time, and must be a whole number of the content's frames */
BOOST_AUTO_TEST_CASE(lua_video_fade_in_ticks)
{
	auto film = new_test_film("lua_video_fade_in_ticks");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	set_global(state, "picture", boost::filesystem::canonical("test/data/flat_red.png").string());
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:add_content(picture)\n"
		"c:set('video.fade_in', dcpomatic.seconds(1))\n"
		"assert(c:get('video.fade_in') == dcpomatic.seconds(1))\n"
		"film:save()\n"
		);

	auto const error = error_from(state, "c:set('video.fade_in', dcpomatic.seconds(1) + 1)");
	BOOST_CHECK_MESSAGE(error.find("property 'video.fade_in': expected a whole number of frames") != string::npos, error);

	auto check = make_shared<Film>(*film->directory());
	check->read_metadata();
	BOOST_REQUIRE_EQUAL(check->content().size(), 1U);
	BOOST_CHECK_EQUAL(check->content()[0]->video->fade_in(), 24);
}


/** new_film makes a film, and refuses to make another in the same directory */
BOOST_AUTO_TEST_CASE(lua_new_film)
{
	auto const dir = test_film_dir("lua_new_film");
	boost::filesystem::remove_all(dir);

	LuaState state;
	set_global(state, "film_dir", dir.string());
	check_runs(state, "dcpomatic.new_film(film_dir)");
	BOOST_CHECK(boost::filesystem::exists(dir / "metadata.xml"));

	auto const error = error_from(state, "dcpomatic.new_film(film_dir)");
	BOOST_CHECK_MESSAGE(error.find("there is already a film in") != string::npos, error);
}


/** audio_channels, audio_frame_rate and video_bit_rate accept only the values that the GUI offers */
BOOST_AUTO_TEST_CASE(lua_film_value_ranges)
{
	auto film = new_test_film("lua_film_value_ranges");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	auto const max_rate = Config::instance()->maximum_video_bit_rate(VideoEncoding::JPEG2000);
	set_global(state, "max_rate", fmt::to_string(max_rate));
	auto const allow_96khz = Config::instance()->allow_96khz_audio();
	dcp::ScopeGuard sg = [allow_96khz]() { Config::instance()->set_allow_96hhz_audio(allow_96khz); };
	Config::instance()->set_allow_96hhz_audio(true);
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:set('audio_channels', 16)\n"
		"film:set('audio_frame_rate', 96000)\n"
		"film:set('video_bit_rate', tonumber(max_rate))\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("film:set('audio_channels', 7)", "property 'audio_channels': expected an even number from 2 to 16");
	check_error("film:set('audio_channels', 18)", "property 'audio_channels': expected an even number from 2 to 16");
	check_error("film:set('audio_frame_rate', 44100)", "property 'audio_frame_rate': expected 48000 or 96000");
	check_error("film:set('video_bit_rate', tonumber(max_rate) + 1)", fmt::format("property 'video_bit_rate': expected a bit rate in bit/s from 1000000 to {}", max_rate));
	check_error("film:set('video_bit_rate', 0)", "property 'video_bit_rate': expected a bit rate in bit/s from 1000000");

	/* The upmixer makes 6 channels, so fewer than that is not allowed */
	check_runs(state, "film:set('audio_processor', 'stereo-5.1-upmix-a')");
	check_error("film:set('audio_channels', 4)", "property 'audio_channels': expected an even number from 6 to 16");

	/* The GUI sets the bit rate in whole Mbit/s */
	check_error("film:set('video_bit_rate', 100500000)", "property 'video_bit_rate': expected a bit rate in bit/s from 1000000 to");
	check_error("film:set('video_bit_rate', 100500000)", "in whole Mbit/s");

	/* 96kHz only when the preferences allow it, as in the GUI */
	Config::instance()->set_allow_96hhz_audio(false);
	check_error("film:set('audio_frame_rate', 96000)", "property 'audio_frame_rate': expected 48000");
	check_runs(state, "film:set('audio_frame_rate', 48000)");
}


/** film:hints() gives the hints that the GUI would show, and make_dcp refuses to run past them unless told to */
BOOST_AUTO_TEST_CASE(lua_hints)
{
	LuaState state;
	picture_and_sound(state, "lua_hints");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		"hints = table.concat(film:hints(), '\\n')\n"
		);
	/* The default of 6 channels gives a hint */
	auto const hints = get_global(state, "hints");
	BOOST_CHECK_MESSAGE(hints.find("rather than 8 or 16") != string::npos, hints);

	auto const error = error_from(state, "film:make_dcp()");
	BOOST_CHECK_MESSAGE(error.find("rather than 8 or 16") != string::npos, error);
	BOOST_CHECK_MESSAGE(error.find("ignore_hints") != string::npos, error);

	/* With nothing to hint about, make_dcp needs no argument */
	check_runs(state,
		"film:set('audio_channels', 8)\n"
		"film:set('audio_language', 'en-US')\n"
		"assert(#film:hints() == 0, table.concat(film:hints(), '\\n'))\n"
		"film:make_dcp()\n"
		);
}


/** make_dcp stops, as the GUI does, when content has changed since it was added */
BOOST_AUTO_TEST_CASE(lua_make_dcp_stops_on_changed_content)
{
	LuaState state;
	picture_and_sound(state, "lua_make_dcp_stops_on_changed_content");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		);

	write_wav(get_global(state, "wav"), 1, 2);

	auto const error = error_from(state, "film:make_dcp{ ignore_hints = true }");
	BOOST_CHECK_MESSAGE(error.find("Files have changed since they were added to the project") != string::npos, error);

	/* The failed job would fail the next test's wait_for_jobs() */
	JobManager::drop();
}


/** make_dcp refuses to write into an existing DCP folder unless told to overwrite it, and then leaves one DCP */
BOOST_AUTO_TEST_CASE(lua_make_dcp_existing_folder)
{
	LuaState state;
	picture_and_sound(state, "lua_make_dcp_existing_folder");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		"film:make_dcp{ ignore_hints = true }\n"
		);

	auto const error = error_from(state, "film:make_dcp{ ignore_hints = true }");
	BOOST_CHECK_MESSAGE(error.find("already exists") != string::npos, error);
	BOOST_CHECK_MESSAGE(error.find("overwrite") != string::npos, error);

	check_runs(state, "film:make_dcp{ ignore_hints = true, overwrite = true }");

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_CHECK_EQUAL(dcp.cpls().size(), 1U);
	BOOST_CHECK_EQUAL(dcp.pkls().size(), 1U);
}


/** A film with no name and no ISDCF name gets the name DCP before its folder is found, as in the GUI,
 *  so overwriting replaces that folder and never the film's own directory.
 */
BOOST_AUTO_TEST_CASE(lua_make_dcp_unnamed_film)
{
	LuaState state;
	picture_and_sound(state, "lua_make_dcp_unnamed_film");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		"film:set('use_isdcf_name', false)\n"
		"film:set('name', '')\n"
		"film:make_dcp{ ignore_hints = true }\n"
		"film:make_dcp{ ignore_hints = true, overwrite = true }\n"
		);

	boost::filesystem::path const dir(get_global(state, "film_dir"));
	BOOST_CHECK(boost::filesystem::exists(dir / "metadata.xml"));
	BOOST_CHECK(boost::filesystem::is_directory(dir / "DCP"));

	/* A DCP name of .. would be the film's parent directory */
	auto const error = error_from(state,
		"film:set('name', '..')\n"
		"film:make_dcp{ ignore_hints = true }\n"
		);
	BOOST_CHECK_MESSAGE(error.find("cannot be used as the name of a DCP folder") != string::npos, error);
}


/** An option that make_dcp does not know raises an error naming it */
BOOST_AUTO_TEST_CASE(lua_make_dcp_unknown_option)
{
	LuaState state;
	picture_and_sound(state, "lua_make_dcp_unknown_option");
	auto const error = error_from(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:make_dcp{ ignore_hints = true, overwrit = true }\n"
		);
	BOOST_CHECK_MESSAGE(error.find("unknown option 'overwrit'") != string::npos, error);
}


/** make_dcp saves the film first, so the saved settings are those of the DCP */
BOOST_AUTO_TEST_CASE(lua_make_dcp_saves)
{
	LuaState state;
	picture_and_sound(state, "lua_make_dcp_saves");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		"film:set('name', 'Saved by make_dcp')\n"
		"film:make_dcp{ ignore_hints = true }\n"
		);

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	BOOST_CHECK_EQUAL(film->name(), "Saved by make_dcp");
	BOOST_CHECK_EQUAL(film->content().size(), 2U);
	BOOST_CHECK(boost::filesystem::is_directory(film->dir(film->dcp_name(), false)));
}


/** The hints of film:hints() and film:make_dcp() are printed to stderr and kept in dcpomatic.warnings() */
BOOST_AUTO_TEST_CASE(lua_warnings_from_hints)
{
	LuaState state;
	picture_and_sound(state, "lua_warnings_from_hints");
	CerrCapture capture;
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		"film:hints()\n"
		"from_hints = table.concat(dcpomatic.warnings(), '\\n')\n"
		"dcpomatic.clear_warnings()\n"
		"assert(#dcpomatic.warnings() == 0)\n"
		"assert(not pcall(function() film:make_dcp() end))\n"
		"from_make_dcp = table.concat(dcpomatic.warnings(), '\\n')\n"
		);

	auto const from_hints = get_global(state, "from_hints");
	BOOST_CHECK_MESSAGE(from_hints.find("rather than 8 or 16") != string::npos, from_hints);
	auto const from_make_dcp = get_global(state, "from_make_dcp");
	BOOST_CHECK_MESSAGE(from_make_dcp.find("rather than 8 or 16") != string::npos, from_make_dcp);
	BOOST_CHECK_MESSAGE(capture.has_warning("rather than 8 or 16"), capture.text());
}


/** The notes that Film::read_metadata gives when opening a film are warnings */
BOOST_AUTO_TEST_CASE(lua_warnings_from_open_notes)
{
	auto film = new_test_film("lua_warnings_from_open_notes", { content_factory("test/data/flat_red.png")[0] });
	film->write_metadata();
	{
		/* Content that does not start on a frame boundary is moved, with a note */
		Editor editor(film->file("metadata.xml"));
		editor.replace("<Position>0</Position>", "<Position>1</Position>");
	}

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	CerrCapture capture;
	check_runs(state,
		"dcpomatic.open_film(film_dir)\n"
		"warnings = table.concat(dcpomatic.warnings(), '\\n')\n"
		);

	auto const warnings = get_global(state, "warnings");
	BOOST_CHECK_MESSAGE(warnings.find("not aligned to a frame boundary") != string::npos, warnings);
	BOOST_CHECK_MESSAGE(capture.has_warning("not aligned to a frame boundary"), capture.text());
}


/** The message that a job leaves for the user is a warning, even when the job then fails */
BOOST_AUTO_TEST_CASE(lua_warnings_from_job)
{
	LuaState state;
	picture_and_sound(state, "lua_warnings_from_job");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		);

	write_wav(get_global(state, "wav"), 1, 2);

	CerrCapture capture;
	auto const error = error_from(state, "film:make_dcp{ ignore_hints = true }");
	BOOST_CHECK_MESSAGE(error.find("Files have changed since they were added to the project") != string::npos, error);
	check_runs(state, "warnings = table.concat(dcpomatic.warnings(), '\\n')");

	auto const warnings = get_global(state, "warnings");
	BOOST_CHECK_MESSAGE(warnings.find("These files will now be re-examined") != string::npos, warnings);
	BOOST_CHECK_MESSAGE(capture.has_warning("These files will now be re-examined"), capture.text());

	/* The failed job would fail the next test's wait_for_jobs() */
	JobManager::drop();
}


/** A message that the film sends through Film::Message, which the GUI shows in a dialog, is a warning */
BOOST_AUTO_TEST_CASE(lua_warnings_from_film_message)
{
	auto film = new_test_film("lua_warnings_from_film_message");

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	/* MPEG2 Interop is offered only when the preferences allow it */
	auto const allow_mpeg2 = Config::instance()->allow_mpeg2();
	dcp::ScopeGuard sg = [allow_mpeg2]() { Config::instance()->set_allow_mpeg2(allow_mpeg2); };
	Config::instance()->set_allow_mpeg2(true);
	CerrCapture capture;
	/* MPEG2 needs a 1920x1080 container, so the film changes it and says so */
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:set('interop', true)\n"
		"film:set('video_encoding', 'mpeg2')\n"
		"warnings = table.concat(dcpomatic.warnings(), '\\n')\n"
		);

	auto const warnings = get_global(state, "warnings");
	BOOST_CHECK_MESSAGE(warnings.find("had to set your container to 1920x1080") != string::npos, warnings);
	BOOST_CHECK_MESSAGE(capture.has_warning("had to set your container to 1920x1080"), capture.text());
}


/** make_dcp of an encrypted film gives the GUI's notice about backing up the metadata */
BOOST_AUTO_TEST_CASE(lua_warnings_from_encrypted_make_dcp)
{
	LuaState state;
	picture_and_sound(state, "lua_warnings_from_encrypted_make_dcp");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:set('encrypt_picture', true)\n"
		"film:set('encrypt_sound', true)\n"
		"film:set('name', 'Encrypted')\n"
		"film:save()\n"
		);

	/* An existing DCP folder stops make_dcp after the notice and before any encoding */
	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	film->set_isdcf_date_today();
	boost::filesystem::create_directories(film->dir(film->dcp_name(), false));

	auto const error = error_from(state, "film:make_dcp{ ignore_hints = true }");
	BOOST_CHECK_MESSAGE(error.find("already exists") != string::npos, error);
	check_runs(state, "warnings = table.concat(dcpomatic.warnings(), '\\n')");
	auto const warnings = get_global(state, "warnings");
	BOOST_CHECK_MESSAGE(warnings.find("You are making an encrypted DCP") != string::npos, warnings);
	BOOST_CHECK_MESSAGE(warnings.find("BACKED UP") != string::npos, warnings);
	BOOST_CHECK_MESSAGE(warnings.find("<") == string::npos, warnings);
}


/** The Config::Bad handler never throws, as an exception would make Config::read reset the configuration */
BOOST_AUTO_TEST_CASE(lua_warnings_from_config_bad)
{
	LuaState state;
	optional<bool> remake;
	BOOST_CHECK_NO_THROW(remake = Config::Bad(Config::BAD_SIGNER_UTF8_STRINGS));
	BOOST_REQUIRE(remake);
	BOOST_CHECK(!*remake);
	BOOST_REQUIRE_EQUAL(state.warnings().size(), 1U);
	BOOST_CHECK_MESSAGE(state.warnings()[0].find("certificate chain") != string::npos, state.warnings()[0]);
}


/** A config.xml that does not load gives the GUI's warning */
BOOST_AUTO_TEST_CASE(lua_warnings_from_config_failed_to_load)
{
	auto const dir = test_film_dir("lua_warnings_from_config_failed_to_load");
	boost::filesystem::remove_all(dir);
	boost::filesystem::create_directories(dir);
	{
		std::ofstream config((dir / "config.xml").string());
		config << "<Config><Version>not a number</Version>";
	}

	ConfigRestorer cr(dir);
	LuaState state;
	BOOST_REQUIRE_EQUAL(state.warnings().size(), 1U);
	BOOST_CHECK_MESSAGE(state.warnings()[0].find("The existing configuration failed to load") != string::npos, state.warnings()[0]);
}


/** An error from the library while a property is set names the property */
BOOST_AUTO_TEST_CASE(lua_negative_trim_names_property)
{
	LuaState state;
	film_and_wav(state, "lua_negative_trim_names_property", 1, 1);
	auto const error = error_from(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(wav):set('trim_end', -1)\n"
		);
	BOOST_CHECK_MESSAGE(error.find("property 'trim_end': ") != string::npos, error);
}


/** @return the <name> of every `void set_<name>(' in the header */
static vector<string>
setters_in(boost::filesystem::path header)
{
	auto const text = dcp::file_to_string(header);
	vector<string> names;
	boost::regex const setter("void\\s+set_(\\w+)\\s*\\(");
	for (boost::sregex_iterator i(text.begin(), text.end(), setter), end; i != end; ++i) {
		names.push_back((*i)[1]);
	}
	return names;
}


/** Every setter of Film, Content and the video, audio and text parts has a row in its table,
 *  or is in that table's list of skipped setters.
 */
BOOST_AUTO_TEST_CASE(lua_every_setter_is_exposed)
{
	auto covered = [](string const& name, string const& prefix, vector<vector<string>> const& tables, vector<SkippedSetter> const& skipped) {
		for (auto const& names: tables) {
			if (std::find(names.begin(), names.end(), prefix + name) != names.end()) {
				return true;
			}
		}
		return std::find_if(skipped.begin(), skipped.end(), [&name](SkippedSetter const& s) { return s.name == name; }) != skipped.end();
	};

	struct Header
	{
		string file;
		string prefix;
		vector<vector<string>> tables;
		vector<SkippedSetter> skipped;
	};

	/* video.fade_in and video.fade_out are content rows, as they need the film for the frame rate */
	vector<Header> const headers = {
		{ "film.h", "", { film_properties().names() }, film_skipped_setters() },
		{ "content.h", "", { content_properties().names() }, content_skipped_setters() },
		{ "video_content.h", "video.", { video_properties().names(), content_properties().names() }, video_skipped_setters() },
		{ "audio_content.h", "audio.", { audio_properties().names() }, audio_skipped_setters() },
		{ "text_content.h", "text.", { text_properties().names() }, text_skipped_setters() },
		{ "ffmpeg_content.h", "ffmpeg.", { ffmpeg_properties().names() }, ffmpeg_skipped_setters() },
		{ "dcp_content.h", "dcp.", { dcp_properties().names() }, dcp_skipped_setters() },
	};

	for (auto const& header: headers) {
		auto const setters = setters_in(boost::filesystem::path("src/lib") / header.file);
		BOOST_REQUIRE_MESSAGE(!setters.empty(), header.file);
		vector<string> missing;
		for (auto const& name: setters) {
			if (!covered(name, header.prefix, header.tables, header.skipped)) {
				missing.push_back(name);
			}
		}
		BOOST_CHECK_MESSAGE(missing.empty(), fmt::format("{}: no row or skipped entry for set_{}", header.file, fmt::join(missing, ", set_")));
	}
}


/** Every public method of Film, Content and their parts that adds, removes, moves, copies, writes or
 *  sends something (that is, an action rather than a set_* setter) has a Lua call, or a reason
 *  why not.  A new method of this kind fails this test until it is listed here, so a new action
 *  of the library cannot be added without deciding what a script does about it.
 */
BOOST_AUTO_TEST_CASE(lua_every_action_is_exposed)
{
	struct Action
	{
		string header;
		string method;
		/* A name in the Lua bindings, or empty if there is none */
		string lua;
		/* If there is no Lua name, why not */
		string why_not;
	};

	vector<Action> const actions = {
		{ "film.h", "send_dcp_to_tms", "send_to_tms", "" },
		{ "film.h", "use_template", "new_film", "" },
		{ "film.h", "read_metadata", "open_film", "" },
		{ "film.h", "write_metadata", "save", "" },
		{ "film.h", "write_template", "save_as_template", "" },
		{ "film.h", "copy_from", "duplicate", "" },
		{ "film.h", "make_kdm", "", "KDM creation is outside the scripting scope (design, non-goals)" },
		{ "film.h", "repeat_content", "repeat_content", "" },
		{ "film.h", "examine_and_add_content", "add_content", "" },
		{ "film.h", "add_content", "add_content", "" },
		{ "film.h", "remove_content", "remove_content", "" },
		{ "film.h", "move_content_earlier", "move_earlier", "" },
		{ "film.h", "move_content_later", "move_later", "" },
		{ "film.h", "clear_markers", "", "unset_marker does it one marker at a time" },
		{ "film.h", "add_ffoc_lfoc", "", "internal: fills in the FFOC and LFOC markers when the DCP is made" },
		{ "film.h", "read_ui_state", "", "internal: the GUI's own state file" },
		{ "film.h", "write_ui_state", "", "internal: the GUI's own state file" },
		{ "film.h", "read_remembered_assets", "", "internal: part of opening a film" },
		{ "film.h", "write_remembered_assets", "", "internal: part of saving a film" },
		{ "film.h", "check_settings_consistency", "", "internal: called by the setters" },
		{ "film.h", "check_reel_boundaries_for_atmos", "", "internal: called by the setters" },
		{ "content.h", "add_path", "", "internal: part of examining content" },
		{ "content.h", "add_properties", "properties", "" },
		{ "video_content.h", "add_properties", "properties", "" },
		{ "audio_content.h", "add_properties", "properties", "" },
		{ "ffmpeg_content.h", "add_properties", "properties", "" },
		{ "dcp_content.h", "add_properties", "properties", "" },
		{ "audio_content.h", "add_stream", "", "internal: part of examining content" },
		{ "text_content.h", "clear_fonts", "", "set_font_file replaces a font" },
		{ "text_content.h", "add_font", "", "set_font_file replaces a font" },
		{ "ffmpeg_content.h", "remove_stream_ids", "", "internal: part of examining content" },
		{ "dcp_content.h", "add_kdm", "add_kdm", "" },
		{ "dcp_content.h", "add_ov", "add_ov", "" },
		{ "dcp_content.h", "check_font_ids", "", "internal: part of examining content" },
		{ "dcp_content.h", "read_directory", "", "internal: part of examining content" },
		{ "dcp_content.h", "read_sub_directory", "", "internal: part of examining content" },
	};

	/* The Lua names are the ones registered in the bindings */
	string bindings;
	for (auto const& file: { "film_binding.cc", "content_binding.cc", "lua_state.cc" }) {
		bindings += dcp::file_to_string(boost::filesystem::path("src/lua") / file);
	}

	for (auto const& action: actions) {
		if (action.lua.empty()) {
			BOOST_CHECK_MESSAGE(!action.why_not.empty(), action.method << " has neither a Lua name nor a reason");
		} else {
			BOOST_CHECK_MESSAGE(
				bindings.find("{ \"" + action.lua + "\", ") != string::npos,
				action.method << ": no Lua call called " << action.lua
				);
		}
	}

	/* A method like these in a header that is not in the list is new */
	boost::regex const action_name(
		"^\\s+(?:virtual\\s+)?[\\w:<>,\\s]+?[\\s&*]+((?:add|remove|move|repeat|clear|copy|send|use|write|read|make|examine|check)_\\w+|"
		"send_dcp_to_tms|use_template|copy_from|repeat_content|write_metadata)\\s*\\([^;{]*\\)[^;{]*[;{]"
		);
	vector<string> const headers = {
		"film.h", "content.h", "video_content.h", "audio_content.h", "text_content.h", "ffmpeg_content.h", "dcp_content.h"
	};
	for (auto const& header: headers) {
		auto const text = dcp::file_to_string(boost::filesystem::path("src/lib") / header);
		vector<string> unlisted;
		std::istringstream lines(text);
		string line;
		while (std::getline(lines, line)) {
			boost::smatch match;
			if (!boost::regex_search(line, match, action_name)) {
				continue;
			}
			string const method = match[1];
			/* The Lua column is for the setters, which lua_every_setter_is_exposed checks;
			 * properties are not actions
			 */
			if (method.rfind("set_", 0) == 0 || (method.rfind("use_", 0) == 0 && method != "use_template")) {
				continue;
			}
			auto const known = std::find_if(actions.begin(), actions.end(), [&](Action const& a) {
				return a.header == header && a.method == method;
			});
			if (known == actions.end() && std::find(unlisted.begin(), unlisted.end(), method) == unlisted.end()) {
				unlisted.push_back(method);
			}
		}
		BOOST_CHECK_MESSAGE(
			unlisted.empty(),
			fmt::format("{}: no Lua call or reason for {}; add it to lua_every_action_is_exposed", header, fmt::join(unlisted, ", "))
			);
	}
}


/** The dynamic range, the PQ settings of the film and the PQ flag of a picture can be read and set */
BOOST_AUTO_TEST_CASE(lua_pq_settings)
{
	LuaState state;
	picture_and_sound(state, "lua_pq_settings");
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"assert(film:get('dynamic_range') == 'sdr', film:get('dynamic_range'))\n"
		"film:set('dynamic_range', 'dci_hdr')\n"
		"assert(film:get('dynamic_range') == 'dci_hdr')\n"
		"film:set('dynamic_range', 'dolby_vision')\n"
		"assert(film:get('dynamic_range') == 'dolby_vision')\n"
		"film:set('pq_target_peak', 100)\n"
		"film:set('pq_sdr_white', 60)\n"
		"film:set('pq_source_peak', 4000)\n"
		"film:set('pq_dci_white', true)\n"
		"assert(film:get('pq_target_peak') == 100)\n"
		"assert(film:get('pq_sdr_white') == 60)\n"
		"assert(film:get('pq_source_peak') == 4000)\n"
		"assert(film:get('pq_dci_white') == true)\n"
		"film:save()\n"
		"local reopened = dcpomatic.open_film(film_dir)\n"
		"assert(reopened:get('dynamic_range') == 'dolby_vision')\n"
		"assert(reopened:get('pq_source_peak') == 4000)\n"
		"local still = reopened:add_content(picture)\n"
		"assert(still:get('video.pq') == false)\n"
		"still:set('video.pq', true)\n"
		"assert(still:get('video.pq') == true)\n"
		);

	auto const error = error_from(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:set('dynamic_range', 'hlg')\n"
		);
	BOOST_CHECK_MESSAGE(error.find("unknown value 'hlg'") != string::npos, error);
}


/** Content lengths before and after the trim, and the film's length, are integer ticks as the timing panel shows them */
BOOST_AUTO_TEST_CASE(lua_content_lengths)
{
	LuaState state;
	picture_and_sound(state, "lua_content_lengths", 20);
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local still = film:add_content(picture)\n"
		"local sound = film:add_content(wav)\n"
		"sound:set('trim_end', dcpomatic.seconds(8))\n"
		"assert(still:full_length() == dcpomatic.seconds(10), still:full_length())\n"
		"assert(still:length_after_trim() == dcpomatic.seconds(10), still:length_after_trim())\n"
		"assert(sound:full_length() == dcpomatic.seconds(20), sound:full_length())\n"
		"assert(math.type(sound:full_length()) == 'integer')\n"
		"assert(sound:length_after_trim() == dcpomatic.seconds(12), sound:length_after_trim())\n"
		"assert(math.type(sound:length_after_trim()) == 'integer')\n"
		"assert(film:length() == dcpomatic.seconds(12), film:length())\n"
		"assert(math.type(film:length()) == 'integer')\n"
		);
}


/** film:reels() gives the reel periods, in DCP ticks, that a by-video-content film will have */
BOOST_AUTO_TEST_CASE(lua_film_reels)
{
	LuaState state;
	picture_and_sound(state, "lua_film_reels");
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(picture)\n"
		"film:set('reel_type', 'by-video-content')\n"
		"local reels = film:reels()\n"
		"assert(#reels == 2, #reels)\n"
		"assert(reels[1].from == 0 and reels[1].to == dcpomatic.seconds(10), reels[1].to)\n"
		"assert(reels[2].from == dcpomatic.seconds(10) and reels[2].to == dcpomatic.seconds(20), reels[2].to)\n"
		"assert(math.type(reels[2].from) == 'integer')\n"
		);
}


/** c:properties() gives what the content's Properties dialog shows */
BOOST_AUTO_TEST_CASE(lua_content_properties)
{
	LuaState state;
	auto const dir = film_and_wav(state, "lua_content_properties", 1, 2);
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local p = film:add_content(wav):properties()\n"
		"film:save()\n"
		"count = #p\n"
		"local function find(name) for _, x in ipairs(p) do if x.name == name then return x end end end\n"
		"local rate = find('Content sample rate')\n"
		"assert(rate.category == 'audio' and rate.value == '48000' and rate.unit == 'Hz', rate.category)\n"
		"local samples = find('Full length in audio samples at content rate')\n"
		"assert(samples.category == 'length' and samples.value == '96000' and samples.unit == '', samples.value)\n"
		"assert(samples.sub_heading == nil)\n"
		);

	auto film = make_shared<Film>(dir);
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_CHECK_EQUAL(get_global(state, "count"), fmt::to_string(film->content()[0]->user_properties(film).size()));
}


/** c:status() says what the content list marks: missing files, a DCP that needs a KDM, a DCP that needs its OV */
BOOST_AUTO_TEST_CASE(lua_content_status)
{
	/* Opening a film with a missing file leaves a failed CheckContentJob */
	DropJobsAtEnd drop;
	auto dcp_film = new_test_film("lua_content_status_dcp", content_factory("test/data/flat_red.png"));
	dcp_film->content()[0]->video->set_length(24);
	dcp_film->set_encrypt_picture(true);
	dcp_film->set_encrypt_sound(true);
	make_and_verify_dcp(dcp_film);
	auto const encrypted = dcp_film->dir(dcp_film->dcp_name());

	/* The same DCP without its picture asset, as a VF is without its OV */
	auto const no_picture = test_film_dir("lua_content_status_no_picture");
	boost::filesystem::remove_all(no_picture);
	boost::filesystem::create_directories(no_picture);
	for (auto const& file: boost::filesystem::directory_iterator(encrypted)) {
		boost::filesystem::copy_file(file.path(), no_picture / file.path().filename());
	}
	boost::filesystem::remove(find_file(no_picture, "j2c_"));
	auto const assetmap = no_picture / "ASSETMAP.xml";
	auto const map = dcp::file_to_string(assetmap);
	auto const without = boost::regex_replace(map, boost::regex("<Asset>(?:(?!</Asset>).)*j2c_.*?</Asset>", boost::regex::perl | boost::regex::mod_s), "");
	BOOST_REQUIRE(without != map);
	dcp::write_string_to_file(without, assetmap);

	auto const wav = test_film_dir("lua_content_status_content") / "sound.wav";
	boost::filesystem::remove_all(wav.parent_path());
	boost::filesystem::create_directories(wav.parent_path());
	write_wav(wav, 1, 1);

	auto film = new_test_film(
		"lua_content_status",
		{ content_factory(wav)[0], content_factory("test/data/flat_red.png")[0], make_shared<DCPContent>(boost::filesystem::canonical(encrypted)), make_shared<DCPContent>(boost::filesystem::canonical(no_picture)) }
		);
	film->write_metadata();
	boost::filesystem::remove(wav);

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	check_runs(state,
		"statuses = {}\n"
		"for _, c in ipairs(dcpomatic.open_film(film_dir):content()) do statuses[c:path()] = table.concat(c:status(), ',') end\n"
		);

	/* c:path() of a DCP is one of the files in its folder */
	auto status_of = [&state](boost::filesystem::path path) {
		set_global(state, "path", boost::filesystem::canonical(path.parent_path()).string() + "/" + path.filename().string());
		check_runs(state,
			"status = 'not in the film'\n"
			"for p, s in pairs(statuses) do if p == path or p:find(path .. '/', 1, true) == 1 then status = s end end\n"
			);
		return get_global(state, "status");
	};
	BOOST_CHECK_EQUAL(status_of(wav), "missing");
	BOOST_CHECK_EQUAL(status_of("test/data/flat_red.png"), "");
	BOOST_CHECK_EQUAL(status_of(encrypted), "needs KDM");
	BOOST_CHECK_EQUAL(status_of(no_picture), "needs KDM,needs OV");
}


/** audio.processing_description is what the audio panel shows, and cannot be set */
BOOST_AUTO_TEST_CASE(lua_audio_processing_description)
{
	LuaState state;
	auto const dir = film_and_wav(state, "lua_audio_processing_description", 1, 1);
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"description = film:add_content(wav):get('audio.processing_description')\n"
		"film:save()\n"
		);
	auto const error = error_from(state, "dcpomatic.open_film(film_dir):content()[1]:set('audio.processing_description', 'x')");
	BOOST_CHECK_MESSAGE(error.find("property 'audio.processing_description' is read-only") != string::npos, error);

	auto film = make_shared<Film>(dir);
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_CHECK_EQUAL(get_global(state, "description"), "Audio will not be resampled");
	BOOST_CHECK_EQUAL(get_global(state, "description"), film->content()[0]->audio->processing_description(film));
}


/** The ISDCF name and DCP name are those the DCP panel displays, with today's date (which make_dcp uses) and every
 *  open-text language when the Config says so; the DCP folder is the folder of the last DCP made, with the film's
 *  stored ISDCF date, as Show DCP opens it, and is not made.
 */
BOOST_AUTO_TEST_CASE(lua_film_names)
{
	auto french = content_factory("test/data/15s.srt")[0];
	auto german = content_factory("test/data/45s.srt")[0];
	auto film = new_test_film("lua_film_names", { content_factory("test/data/flat_red.png")[0], french, german });
	french->only_text()->set_use(true);
	french->only_text()->set_language(dcp::LanguageTag("fr"));
	german->only_text()->set_use(true);
	german->only_text()->set_language(dcp::LanguageTag("de"));
	german->only_text()->set_language_is_additional(true);
	film->set_name("Names");
	film->set_use_isdcf_name(true);
	film->set_isdcf_date(boost::gregorian::date(2020, 1, 1));
	film->write_metadata();

	auto const multiple = Config::instance()->add_multiple_open_text_languages_to_isdcf_name();
	Config::instance()->set_add_multiple_open_text_languages_to_isdcf_name(true);
	dcp::ScopeGuard sg = [multiple]() { Config::instance()->set_add_multiple_open_text_languages_to_isdcf_name(multiple); };

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"isdcf_name = film:isdcf_name()\n"
		"dcp_name = film:dcp_name()\n"
		"dcp_folder = film:dcp_folder()\n"
		);

	auto const today = boost::gregorian::to_iso_string(boost::gregorian::day_clock::local_day());
	auto const isdcf_name = get_global(state, "isdcf_name");
	BOOST_CHECK_EQUAL(isdcf_name, film->isdcf_name(true, true));
	BOOST_CHECK_MESSAGE(isdcf_name.find("Names_") == 0, isdcf_name);
	BOOST_CHECK_MESSAGE(isdcf_name.find("_" + today + "_") != string::npos, isdcf_name);
	BOOST_CHECK_MESSAGE(isdcf_name.find("-FR-DE") != string::npos, isdcf_name);

	auto const dcp_name = get_global(state, "dcp_name");
	BOOST_CHECK_EQUAL(dcp_name, film->dcp_name(true, true));
	BOOST_CHECK_MESSAGE(dcp_name.find("_" + today + "_") != string::npos, dcp_name);
	BOOST_CHECK_MESSAGE(dcp_name.find("-FR-DE") != string::npos, dcp_name);

	auto const dcp_folder = get_global(state, "dcp_folder");
	BOOST_CHECK_EQUAL(dcp_folder, (*film->directory() / film->dcp_name(false)).string());
	BOOST_CHECK_MESSAGE(dcp_folder.find("_20200101_") != string::npos, dcp_folder);
	BOOST_CHECK_MESSAGE(dcp_folder.find("-FR-DE") == string::npos, dcp_folder);
	BOOST_CHECK(!boost::filesystem::exists(dcp_folder));
}


/** A content whose font file has gone is marked missing, as the content list does */
BOOST_AUTO_TEST_CASE(lua_content_status_missing_font)
{
	auto const font = test_film_dir("lua_content_status_missing_font_content") / "font.ttf";
	boost::filesystem::remove_all(font.parent_path());
	boost::filesystem::create_directories(font.parent_path());
	boost::filesystem::copy_file("test/data/Inconsolata-VF.ttf", font);

	auto subtitles = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("lua_content_status_missing_font", { subtitles });
	BOOST_REQUIRE(!subtitles->only_text()->fonts().empty());
	subtitles->only_text()->fonts().front()->set_file(font);
	film->write_metadata();

	LuaState state;
	set_global(state, "film_dir", film->directory()->string());
	check_runs(state, "before = table.concat(dcpomatic.open_film(film_dir):content()[1]:status(), ',')");
	boost::filesystem::remove(font);
	check_runs(state, "after = table.concat(dcpomatic.open_film(film_dir):content()[1]:status(), ',')");

	BOOST_CHECK_EQUAL(get_global(state, "before"), "");
	BOOST_CHECK_EQUAL(get_global(state, "after"), "missing");
}


/** describe() gives the lengths, reels, names, properties, status and audio processing that the GUI shows */
BOOST_AUTO_TEST_CASE(lua_describe_lists_facts)
{
	LuaState state;
	picture_and_sound(state, "lua_describe_lists_facts", 20);
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav):set('trim_end', dcpomatic.seconds(8))\n"
		"film:set('name', 'Facts')\n"
		"description = film:describe()\n"
		"dcp_name = film:dcp_name()\n"
		"isdcf_name = film:isdcf_name()\n"
		"dcp_folder = film:dcp_folder()\n"
		);
	auto const description = get_global(state, "description");
	auto has = [&description](string const& line) {
		BOOST_CHECK_MESSAGE(description.find(line) != string::npos, fmt::format("'{}' not in\n{}", line, description));
	};
	has("\nlength: 1152000\n");
	has("\nreels: {from=0, to=1152000}\n");
	has("\nisdcf_name: " + get_global(state, "isdcf_name") + "\n");
	has(
		"\ndcp_name: " + get_global(state, "dcp_name") +
		" (as displayed, with today's date, which make_dcp uses; not the folder name when several open-text languages are in the ISDCF name)\n"
	   );
	has("\ndcp_folder: " + get_global(state, "dcp_folder") + " (folder of the last DCP made, as Show DCP opens it)\n");
	has("\nfull_length: 960000\n");
	has("\nfull_length: 1920000\n");
	has("\nlength_after_trim: 1152000\n");
	has("\nstatus: \n");
	has("\nproperty: {category=audio, name=Content sample rate, unit=Hz, value=48000}\n");
	has("\naudio.processing_description: Audio will not be resampled\n");
}


/** film:add_content() takes a DCP folder as the GUI's Add DCP button does; its video length cannot be set */
BOOST_AUTO_TEST_CASE(lua_add_dcp_content)
{
	LuaState state;
	picture_and_sound(state, "lua_add_dcp_content");
	auto const vf = test_film_dir("lua_add_dcp_content_vf");
	boost::filesystem::remove_all(vf);
	set_global(state, "vf_dir", vf.string());
	check_runs(state,
		"local ov = dcpomatic.open_film(film_dir)\n"
		"ov:add_content(picture)\n"
		"ov:make_dcp{ ignore_hints = true }\n"
		"film = dcpomatic.new_film(vf_dir)\n"
		"dcp = film:add_content(ov:dcp_folder())\n"
		"assert(#film:content() == 1, #film:content())\n"
		"assert(dcp:full_length() == dcpomatic.seconds(10), dcp:full_length())\n"
		"assert(#dcp:status() == 0, table.concat(dcp:status(), ','))\n"
		"assert(not film:describe():find('video.length', 1, true))\n"
		"film:save()\n"
		);

	auto const set_error = error_from(state, "dcp:set('video.length', dcpomatic.seconds(5))");
	BOOST_CHECK_MESSAGE(set_error.find("property 'video.length': only still image content") != string::npos, set_error);
	auto const get_error = error_from(state, "dcp:get('video.length')");
	BOOST_CHECK_MESSAGE(get_error.find("property 'video.length': only still image content") != string::npos, get_error);

	auto film = make_shared<Film>(vf);
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_CHECK(std::dynamic_pointer_cast<DCPContent>(film->content()[0]));
}


/** film:move_earlier() and film:move_later() swap content as the GUI's Earlier and Later buttons do */
BOOST_AUTO_TEST_CASE(lua_move_content)
{
	LuaState state;
	picture_and_sound(state, "lua_move_content");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"local a = film:add_content(picture)\n"
		"local b = film:add_content(picture)\n"
		"film:move_later(a)\n"
		"assert(a:get('position') == dcpomatic.seconds(10), a:get('position'))\n"
		"assert(b:get('position') == 0, b:get('position'))\n"
		"assert(a:index() == 2 and b:index() == 1, a:index())\n"
		"film:move_earlier(a)\n"
		"assert(a:get('position') == 0, a:get('position'))\n"
		"assert(b:get('position') == dcpomatic.seconds(10), b:get('position'))\n"
		"assert(a:index() == 1 and b:index() == 2, a:index())\n"
		);

	auto const vf = test_film_dir("lua_move_content_other");
	boost::filesystem::remove_all(vf);
	set_global(state, "other_dir", vf.string());
	auto const error = error_from(state, "dcpomatic.new_film(other_dir):move_later(film:content()[1])");
	BOOST_CHECK_MESSAGE(error.find("film:move_later: argument 2: the content is in another film") != string::npos, error);
}


/** film:repeat_content() repeats content as the GUI's Repeat dialog does, 1 to 1024 times */
BOOST_AUTO_TEST_CASE(lua_repeat_content)
{
	LuaState state;
	picture_and_sound(state, "lua_repeat_content");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"still = film:add_content(picture)\n"
		"film:repeat_content({ still }, 2)\n"
		"assert(#film:content() == 3, #film:content())\n"
		"assert(film:length() == dcpomatic.seconds(30), film:length())\n"
		"assert(film:content()[3]:get('position') == dcpomatic.seconds(20), film:content()[3]:get('position'))\n"
		);

	auto const zero = error_from(state, "film:repeat_content({ still }, 0)");
	BOOST_CHECK_MESSAGE(zero.find("film:repeat_content: argument 3: expected 1 to 1024") != string::npos, zero);
	auto const too_many = error_from(state, "film:repeat_content({ still }, 1025)");
	BOOST_CHECK_MESSAGE(too_many.find("film:repeat_content: argument 3: expected 1 to 1024") != string::npos, too_many);
	auto const empty = error_from(state, "film:repeat_content({}, 1)");
	BOOST_CHECK_MESSAGE(empty.find("film:repeat_content: argument 2: the list is empty, so there is nothing to repeat") != string::npos, empty);
	auto const not_list = error_from(state, "film:repeat_content(still, 1)");
	BOOST_CHECK_MESSAGE(not_list.find("film:repeat_content: argument 2: expected a list of Content") != string::npos, not_list);
	auto const not_content = error_from(state, "film:repeat_content({ 1 }, 1)");
	BOOST_CHECK_MESSAGE(not_content.find("film:repeat_content: argument 2, item 1:") != string::npos, not_content);
}


/** c:reexamine() examines content again, as the GUI's Re-examine does, and waits for it */
BOOST_AUTO_TEST_CASE(lua_reexamine_content)
{
	LuaState state;
	film_and_wav(state, "lua_reexamine_content", 1, 1);
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"sound = film:add_content(wav)\n"
		"assert(sound:full_length() == dcpomatic.seconds(1), sound:full_length())\n"
		);

	write_wav(get_global(state, "wav"), 1, 2);

	check_runs(state,
		"assert(sound:full_length() == dcpomatic.seconds(1), sound:full_length())\n"
		"sound:reexamine()\n"
		"assert(sound:full_length() == dcpomatic.seconds(2), sound:full_length())\n"
		);

	auto const error = error_from(state, "film:remove_content(sound)\nsound:reexamine()");
	BOOST_CHECK_MESSAGE(error.find("c:reexamine: content was removed from the film") != string::npos, error);
}


/** film:find_missing() looks for moved content in a folder, as the GUI's Find missing does, and says how many it found */
BOOST_AUTO_TEST_CASE(lua_find_missing)
{
	/* Opening a film with a missing file leaves a failed CheckContentJob */
	DropJobsAtEnd drop;
	LuaState state;
	film_and_wav(state, "lua_find_missing", 1, 1);
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(wav)\n"
		"film:save()\n"
		);

	auto const moved = test_film_dir("lua_find_missing_moved");
	boost::filesystem::remove_all(moved);
	boost::filesystem::create_directories(moved / "deeper");
	boost::filesystem::rename(get_global(state, "wav"), moved / "deeper" / "sound.wav");
	set_global(state, "moved", moved.string());
	/* build may be a link, which the search resolves */
	set_global(state, "moved_wav", boost::filesystem::canonical(moved / "deeper" / "sound.wav").string());

	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local sound = film:content()[1]\n"
		"assert(table.concat(sound:status(), ',') == 'missing', table.concat(sound:status(), ','))\n"
		"local found = film:find_missing(moved)\n"
		"assert(found == 1, found)\n"
		"assert(#sound:status() == 0, table.concat(sound:status(), ','))\n"
		"assert(sound:path() == moved_wav, sound:path())\n"
		"assert(film:find_missing(moved) == 0)\n"
		);
}


/** video.length of still image content is its length in ticks, set as the timing panel's full length does */
BOOST_AUTO_TEST_CASE(lua_still_length)
{
	LuaState state;
	picture_and_sound(state, "lua_still_length");
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"local still = film:add_content(picture)\n"
		"assert(still:get('video.length') == dcpomatic.seconds(10), still:get('video.length'))\n"
		"still:set('video.length', dcpomatic.seconds(5))\n"
		"assert(still:get('video.length') == dcpomatic.seconds(5), still:get('video.length'))\n"
		"assert(math.type(still:get('video.length')) == 'integer')\n"
		"assert(still:full_length() == dcpomatic.seconds(5), still:full_length())\n"
		"assert(film:length() == dcpomatic.seconds(5), film:length())\n"
		"assert(film:describe():find('\\nvideo.length: ' .. dcpomatic.seconds(5) .. '\\n', 1, true))\n"
		"film:save()\n"
		);

	auto const error = error_from(state, "dcpomatic.open_film(film_dir):add_content(wav):set('video.length', 1)");
	BOOST_CHECK_MESSAGE(error.find("property 'video.length': the content has no video") != string::npos, error);
	auto const negative = error_from(state, "dcpomatic.open_film(film_dir):content()[1]:set('video.length', -1)");
	BOOST_CHECK_MESSAGE(negative.find("property 'video.length': expected a length of 0 or more") != string::npos, negative);

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_CHECK_EQUAL(film->content()[0]->video->length(), 5 * film->video_frame_rate());
}


/** film:join() joins FFmpeg content as the GUI's Join does; the old handles then raise, and the library's refusals are errors */
BOOST_AUTO_TEST_CASE(lua_join_content)
{
	LuaState state;
	/* Joining reads the files as one stream, as for VOBs, so they must be a format that can be cut like that */
	film_and_files(state, "lua_join_content", { { "red", "count300bd24.m2ts" }, { "red30", "count300bd48.m2ts" }, { "picture", "flat_red.png" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"a = film:add_content(red)\n"
		"local b = film:add_content(red)\n"
		"assert(a:full_length() == 1200000, a:full_length())\n"
		"joined = film:join({ a, b })\n"
		"assert(#film:content() == 1, #film:content())\n"
		"assert(joined:index() == 1)\n"
		"assert(joined:path() == red)\n"
		/* The same file twice repeats its timestamps, so the joined stream ends where one copy does */
		"assert(joined:full_length() == 1200000, joined:full_length())\n"
		"film:save()\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("a:get('position')", "content was removed from the film");
	check_error("film:join({ joined })", "film:join: argument 2: expected a list of two or more pieces of content");
	check_error("film:join({ joined, film:add_content(picture) })", "film:join: argument 2, item 2: only content read by FFmpeg (video and sound files) can be joined");
	check_error("film:join({ joined, 1 })", "film:join: argument 2, item 2: expected a Content");
	check_error("film:join({ 1, joined })", "film:join: argument 2, item 1: expected a Content");
	check_error("film:join({ joined, joined })", "film:join: argument 2, item 2: the content is in the list twice");
	check_error("film:join(joined)", "film:join: argument 2: expected a list of two or more pieces of content");
	check_error("other = film:add_content(red30)\nfilm:join({ joined, other })", "film:join: Content to be joined must have the same video frame rate");
	/* A refused join leaves the content as it was */
	check_runs(state, "assert(#film:content() == 3, #film:content())\nassert(joined:index() == 1)\n");

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	BOOST_CHECK_EQUAL(film->content()[0]->paths().size(), 2U);
}


/** ffmpeg.filters is the list of the ids of the video filters of FFmpeg content, as the Advanced dialog sets them */
BOOST_AUTO_TEST_CASE(lua_ffmpeg_filters)
{
	LuaState state;
	film_and_files(state, "lua_ffmpeg_filters", { { "red", "red_24.mp4" }, { "picture", "flat_red.png" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:add_content(red)\n"
		"assert(#c:get('ffmpeg.filters') == 0)\n"
		"c:set('ffmpeg.filters', { 'vflip', 'hflip' })\n"
		"local f = c:get('ffmpeg.filters')\n"
		"assert(#f == 2 and f[1] == 'vflip' and f[2] == 'hflip', table.concat(f, ','))\n"
		"assert(film:describe():find('\\nffmpeg.filters: vflip, hflip\\n', 1, true), film:describe())\n"
		"film:save()\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("c:set('ffmpeg.filters', { 'nope' })", "property 'ffmpeg.filters': unknown filter 'nope'; the filters are vflip, hflip");
	check_error("c:set('ffmpeg.filters', 'vflip')", "property 'ffmpeg.filters': expected a list of filter ids");
	check_error("film:add_content(picture):get('ffmpeg.filters')", "property 'ffmpeg.filters': the content is not read by FFmpeg");

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	auto ffmpeg = std::dynamic_pointer_cast<FFmpegContent>(film->content()[0]);
	BOOST_REQUIRE(ffmpeg);
	BOOST_REQUIRE_EQUAL(ffmpeg->filters().size(), 2U);
	BOOST_CHECK_EQUAL(ffmpeg->filters()[0].id(), "vflip");
	BOOST_CHECK_EQUAL(ffmpeg->filters()[1].id(), "hflip");
}


/** ffmpeg.subtitle_stream is the identifier of the subtitle stream used, chosen from c:subtitle_streams() as the text panel does */
BOOST_AUTO_TEST_CASE(lua_ffmpeg_subtitle_stream)
{
	LuaState state;
	film_and_files(state, "lua_ffmpeg_subtitle_stream", { { "m2ts", "count300bd24.m2ts" }, { "red", "red_24.mp4" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:add_content(m2ts)\n"
		"local streams = c:subtitle_streams()\n"
		"assert(#streams == 1, #streams)\n"
		"assert(type(streams[1].name) == 'string')\n"
		"assert(c:get('ffmpeg.subtitle_stream') == streams[1].id, c:get('ffmpeg.subtitle_stream'))\n"
		"c:set('ffmpeg.subtitle_stream', streams[1].id)\n"
		"assert(film:describe():find('\\nffmpeg.subtitle_stream: ' .. streams[1].id .. '\\n', 1, true), film:describe())\n"
		);

	auto const error = error_from(state, "c:set('ffmpeg.subtitle_stream', 'nope')");
	BOOST_CHECK_MESSAGE(error.find("property 'ffmpeg.subtitle_stream': no subtitle stream 'nope'; the streams are ") != string::npos, error);
	auto const none = error_from(state, "film:add_content(red):set('ffmpeg.subtitle_stream', 'nope')");
	BOOST_CHECK_MESSAGE(none.find("property 'ffmpeg.subtitle_stream': no subtitle stream 'nope'; the content has none") != string::npos, none);
	check_runs(state, "assert(film:content()[2]:get('ffmpeg.subtitle_stream') == nil)\nassert(#film:content()[2]:subtitle_streams() == 0)\n");

	/* Only this part proves that setting the row changes the stream: the public file has one stream */
	auto const sintel = TestPaths::private_data() / "Sintel_Trailer1.480p.DivX_Plus_HD.mkv";
	if (!boost::filesystem::exists(sintel)) {
		skipping("Skipping the change of stream in lua_ffmpeg_subtitle_stream: no private test data (Sintel_Trailer1.480p.DivX_Plus_HD.mkv)");
		return;
	}

	set_global(state, "sintel", sintel.string());
	check_runs(state,
		"local s = film:add_content(sintel)\n"
		"local streams = s:subtitle_streams()\n"
		"assert(#streams == 8, #streams)\n"
		"assert(s:get('ffmpeg.subtitle_stream') == streams[1].id)\n"
		"s:set('ffmpeg.subtitle_stream', streams[2].id)\n"
		"assert(s:get('ffmpeg.subtitle_stream') == streams[2].id, s:get('ffmpeg.subtitle_stream'))\n"
		);
}


/** c:subtitle_colours() finds the colours of bitmap subtitles as the appearance dialog does, and c:set_subtitle_colour() changes one */
BOOST_AUTO_TEST_CASE(lua_subtitle_colours)
{
	LuaState state;
	film_and_files(state, "lua_subtitle_colours", { { "m2ts", "count300bd24.m2ts" }, { "red", "red_24.mp4" }, { "picture", "flat_red.png" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:add_content(m2ts)\n"
		"c:set('text.use', true)\n"
		"local colours = c:subtitle_colours()\n"
		"assert(#colours == 109, #colours)\n"
		"from = colours[1].from\n"
		"assert(from == '00000000', from)\n"
		"assert(colours[2].from == '00000012', colours[2].from)\n"
		"assert(colours[1].to == from, colours[1].to)\n"
		"c:set_subtitle_colour(from, 'ff000080')\n"
		"local again = c:subtitle_colours()\n"
		"assert(#again == #colours)\n"
		"assert(again[1].from == from and again[1].to == 'FF000080', again[1].to)\n"
		"assert(film:describe():find('\\nsubtitle colour: ' .. from .. ' to FF000080\\n', 1, true), film:describe())\n"
		"film:save()\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("c:set_subtitle_colour('red', 'FF000080')", "c:set_subtitle_colour: argument 2: expected a colour as 8 hex digits RRGGBBAA, such as 'FF000080'");
	check_error("c:set_subtitle_colour(from, 'FF0000')", "c:set_subtitle_colour: argument 3: expected a colour as 8 hex digits RRGGBBAA");
	check_error("c:set_subtitle_colour('01020304', 'FF000080')", "c:set_subtitle_colour: argument 2: 01020304 is not a colour of the subtitles");
	check_error("film:add_content(red):subtitle_colours()", "c:subtitle_colours: the content has no subtitle stream");
	check_error("film:add_content(picture):subtitle_colours()", "c:subtitle_colours: the content is not read by FFmpeg");
	check_error("c:set('text.use', false)\nc:subtitle_colours()", "c:subtitle_colours: the subtitles must be used as open subtitles or open captions");

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	BOOST_REQUIRE(!film->content().empty());
	auto ffmpeg = std::dynamic_pointer_cast<FFmpegContent>(film->content()[0]);
	BOOST_REQUIRE(ffmpeg && ffmpeg->subtitle_stream());
	auto const colours = ffmpeg->subtitle_stream()->colours();
	BOOST_REQUIRE(!colours.empty());
	auto const to = colours.begin()->second;
	BOOST_CHECK_EQUAL(to.r, 255);
	BOOST_CHECK_EQUAL(to.g, 0);
	BOOST_CHECK_EQUAL(to.b, 0);
	BOOST_CHECK_EQUAL(to.a, 128);
}


/** c:fonts() lists the fonts of a text as the fonts dialog does, and c:set_font_file() gives one a file */
BOOST_AUTO_TEST_CASE(lua_fonts)
{
	LuaState state;
	film_and_wav(state, "lua_fonts", 1, 1);
	set_global(state, "srt", boost::filesystem::canonical("test/data/15s.srt").string());
	set_global(state, "font", boost::filesystem::canonical("test/data/Inconsolata-VF.ttf").string());
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:add_content(srt)\n"
		"local fonts = c:fonts()\n"
		"assert(#fonts == 1, #fonts)\n"
		"id = fonts[1].id\n"
		"assert(type(id) == 'string')\n"
		"assert(fonts[1].file == nil)\n"
		"c:set_font_file(id, font)\n"
		"assert(c:fonts()[1].file == font, c:fonts()[1].file)\n"
		"assert(film:describe():find('\\ntext.font: {file=' .. font .. ', id=' .. id .. '}\\n', 1, true), film:describe())\n"
		"film:save()\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("c:set_font_file('nope', font)", "c:set_font_file: argument 2: no font 'nope'");
	check_error("c:set_font_file(id, '/no/such/font.ttf')", "c:set_font_file: argument 3: /no/such/font.ttf does not exist");
	check_error("c:fonts('closed-caption')", "c:fonts: the content has no closed-caption text");
	check_error("c:set('text.type', 'closed-caption')\nc:set_font_file(id, font)", "c:set_font_file: fonts can be set only for open subtitles and open captions");
	check_runs(state, "c:set('text.type', 'open-subtitle')");
	set_global(state, "m2ts", boost::filesystem::canonical("test/data/count300bd24.m2ts").string());
	check_error("film:add_content(m2ts):set_font_file('', font)", "c:set_font_file: the GUI sets no fonts for bitmap subtitles or for subtitles inside a video file");
	check_error("film:add_content(wav):fonts()", "c:fonts: the content has no text");
	check_error("film:remove_content(c)\nc:fonts()", "c:fonts: content was removed from the film");

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	auto fonts = film->content()[0]->only_text()->fonts();
	BOOST_REQUIRE_EQUAL(fonts.size(), 1U);
	BOOST_REQUIRE(fonts.front()->file());
	BOOST_CHECK_EQUAL(fonts.front()->file()->string(), get_global(state, "font"));
}


/** c:auto_crop{ time = ticks } crops by brightness with the preferences' threshold, as the Auto-crop dialog does with OK */
BOOST_AUTO_TEST_CASE(lua_auto_crop)
{
	LuaState state;
	film_and_wav(state, "lua_auto_crop", 1, 1);
	set_global(state, "dcp", boost::filesystem::canonical("test/data/scaling_test_133_185").string());
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:add_content(dcp)\n"
		"local crop = c:auto_crop{ time = 0 }\n"
		"assert(crop.left == 279 and crop.right == 279 and crop.top == 0 and crop.bottom == 0, crop.left .. ' ' .. crop.right .. ' ' .. crop.top .. ' ' .. crop.bottom)\n"
		"assert(c:get('video.left_crop') == 279 and c:get('video.right_crop') == 279, c:get('video.left_crop'))\n"
		"assert(c:get('video.top_crop') == 0 and c:get('video.bottom_crop') == 0)\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("c:auto_crop()", "c:auto_crop: argument 2: expected { time = ticks }");
	check_error("c:auto_crop{ time = 0.5 }", "c:auto_crop: option 'time': expected integer ticks");
	check_error("c:auto_crop{ time = 0, threshold = 1 }", "c:auto_crop: unknown option 'threshold'");
	check_error("film:add_content(wav):auto_crop{ time = 0 }", "c:auto_crop: the content has no video");
	/* Past the end there is no frame to guess from, so the crop is left as it is */
	check_error(
		"c:set('video.left_crop', 5)\nc:auto_crop{ time = c:full_length() }",
		"c:auto_crop: option 'time': expected less than the length of the video, 96000 ticks"
		);
	check_runs(state, "assert(c:get('video.left_crop') == 5, c:get('video.left_crop'))\nassert(c:get('video.right_crop') == 279)\n");
	check_error("c:set('video.use', false)\nc:auto_crop{ time = 0 }", "c:auto_crop: the content's video is not used");
}


/** c:copy_settings_from() copies the video, audio or text settings of other content, as Paste does */
BOOST_AUTO_TEST_CASE(lua_copy_settings)
{
	LuaState state;
	film_and_files(state, "lua_copy_settings", { { "clip", "red_24_with_sine.mp4" }, { "srt", "15s.srt" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"a = film:add_content(clip)\n"
		"b = film:add_content(clip)\n"
		"a:set('video.left_crop', 10)\n"
		"a:set('audio.gain', 3.0)\n"
		"b:copy_settings_from(a, { video = true })\n"
		"assert(b:get('video.left_crop') == 10, b:get('video.left_crop'))\n"
		"assert(b:get('audio.gain') == 0, b:get('audio.gain'))\n"
		"b:copy_settings_from(a, { audio = true, video = false })\n"
		"assert(b:get('audio.gain') == 3, b:get('audio.gain'))\n"
		"s1 = film:add_content(srt)\n"
		"local s2 = film:add_content(srt)\n"
		"s1:set('text.x_offset', 0.25)\n"
		"s2:copy_settings_from(s1, { text = true })\n"
		"assert(s2:get('text.x_offset') == 0.25, s2:get('text.x_offset'))\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("b:copy_settings_from(a, { colour = true })", "c:copy_settings_from: unknown option 'colour'");
	check_error("b:copy_settings_from(a)", "c:copy_settings_from: argument 3: say which settings to copy, as { video = true, audio = true, text = true }");
	check_error("b:copy_settings_from(a, { text = true })", "c:copy_settings_from: argument 2: the content has no text");
	check_error("b:copy_settings_from(s1, { video = true })", "c:copy_settings_from: argument 2: the content has no video");
	check_error("s1:copy_settings_from(a, { audio = true })", "c:copy_settings_from: the content has no audio");
	check_error("b:copy_settings_from(1, { video = true })", "c:copy_settings_from: argument 2: expected a Content");
	check_error("film:remove_content(a)\nb:copy_settings_from(a, { video = true })", "c:copy_settings_from: content was removed from the film");

	auto const two_texts = TestPaths::private_data() / "two_text_tracks";
	if (!boost::filesystem::exists(two_texts)) {
		skipping("Skipping the different numbers of texts in lua_copy_settings: no private test data (two_text_tracks)");
		return;
	}
	set_global(state, "two_texts", two_texts.string());
	check_error(
		"local dcp = film:add_content(two_texts)\n"
		"local before = dcp:get('text.x_offset', 'open-subtitle')\n"
		"local ok, e = pcall(function() dcp:copy_settings_from(s1, { text = true }) end)\n"
		"assert(dcp:get('text.x_offset', 'open-subtitle') == before)\n"
		"error(e)\n",
		"c:copy_settings_from: the content has 2 texts and argument 2 has 1; texts are copied in pairs, so the numbers must be the same"
		);
}

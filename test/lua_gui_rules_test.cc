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


/** @file  test/lua_gui_rules_test.cc
 *  @brief Tests that Lua scripts are refused the DCP settings that the DCP panel does not offer or disables.
 *  @ingroup feature
 */


#include "lib/config.h"
#include "lua/lua_state.h"
#include "lua_test_helpers.h"
#include <dcp/scope_guard.h>
#include <boost/test/unit_test.hpp>
#include <string>


using std::string;


/** @return a function that checks that code raises an error containing expected */
static auto
error_checker(LuaState& state)
{
	return [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};
}


/** Set the two preferences that the DCP panel's list of standards depends on, and put them back at the end of the scope */
static dcp::ScopeGuard
standard_preferences(bool allow_mpeg2, bool allow_all_smpte_profiles)
{
	auto config = Config::instance();
	auto const old_mpeg2 = config->allow_mpeg2();
	auto const old_profiles = config->allow_all_smpte_profiles();
	config->set_allow_mpeg2(allow_mpeg2);
	config->set_allow_all_smpte_profiles(allow_all_smpte_profiles);
	return [old_mpeg2, old_profiles]() {
		Config::instance()->set_allow_mpeg2(old_mpeg2);
		Config::instance()->set_allow_all_smpte_profiles(old_profiles);
	};
}


/** Interop is not offered for a film with Atmos content */
BOOST_AUTO_TEST_CASE(lua_gui_rules_interop_atmos)
{
	auto guard = standard_preferences(false, false);
	LuaState state;
	film_and_files(state, "lua_gui_rules_interop_atmos", { { "atmos", "atmos_0.mxf" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:set('interop', true)\n"
		"film:set('interop', false)\n"
		"film:add_content(atmos)\n"
		);
	auto check_error = error_checker(state);
	check_error("film:set('interop', true)", "property 'interop': the DCP panel does not offer Interop: the film has Atmos content, which Interop cannot carry");
	check_runs(state, "assert(film:get('interop') == false)\nfilm:set('interop', false)\n");
}


/** MPEG2 Interop is offered only when the preferences allow it, and MPEG2 is always Interop */
BOOST_AUTO_TEST_CASE(lua_gui_rules_mpeg2)
{
	auto guard = standard_preferences(false, false);
	LuaState state;
	film_and_files(state, "lua_gui_rules_mpeg2", {});
	check_runs(state, "film = dcpomatic.open_film(film_dir)\nfilm:set('interop', true)\n");
	auto check_error = error_checker(state);
	check_error(
		"film:set('video_encoding', 'mpeg2')",
		"property 'video_encoding': the DCP panel does not offer MPEG2 Interop: it needs \"Allow use of MPEG2 Interop\" in the preferences"
		);
	check_runs(state, "assert(film:get('video_encoding') == 'jpeg2000')\n");

	Config::instance()->set_allow_mpeg2(true);
	check_runs(state, "film:set('interop', false)\n");
	check_error("film:set('video_encoding', 'mpeg2')", "property 'video_encoding': the DCP panel does not offer MPEG2 with SMPTE: set interop to true first");
	check_runs(state,
		"film:set('interop', true)\n"
		"film:set('video_encoding', 'mpeg2')\n"
		"assert(film:get('video_encoding') == 'mpeg2')\n"
		);
	check_error("film:set('interop', false)", "property 'interop': the DCP panel does not offer MPEG2 with SMPTE: set video_encoding to jpeg2000 first");
	check_runs(state, "film:set('video_encoding', 'jpeg2000')\nfilm:set('interop', false)\n");
}


/** SMPTE A and Bv2.0 are offered only when the preferences allow them */
BOOST_AUTO_TEST_CASE(lua_gui_rules_smpte_profiles)
{
	auto guard = standard_preferences(false, false);
	LuaState state;
	film_and_files(state, "lua_gui_rules_smpte_profiles", {});
	check_runs(state, "film = dcpomatic.open_film(film_dir)\n");
	auto check_error = error_checker(state);
	check_error(
		"film:set('smpte_profile', 'smpte-a')",
		"property 'smpte_profile': the DCP panel does not offer SMPTE (A only): it needs \"Allow limiting DCPs to SMPTE A or Bv2.0\" in the preferences"
		);
	check_error(
		"film:set('smpte_profile', 'smpte-bv20')",
		"property 'smpte_profile': the DCP panel does not offer SMPTE (Bv2.0 only): it needs \"Allow limiting DCPs to SMPTE A or Bv2.0\" in the preferences"
		);
	check_runs(state, "assert(film:get('smpte_profile') == 'smpte-bv21')\nfilm:set('smpte_profile', 'smpte-bv21')\n");

	Config::instance()->set_allow_all_smpte_profiles(true);
	check_runs(state,
		"film:set('smpte_profile', 'smpte-a')\n"
		"film:set('smpte_profile', 'smpte-bv20')\n"
		"assert(film:get('smpte_profile') == 'smpte-bv20')\n"
		);
}


/** While the film references a DCP's video or audio, the DCP panel disables the settings that would stop the reference working */
BOOST_AUTO_TEST_CASE(lua_gui_rules_referenced)
{
	auto guard = standard_preferences(false, false);
	LuaState state;
	picture_and_sound(state, "lua_gui_rules_referenced");
	auto const vf = test_film_dir("lua_gui_rules_referenced_vf");
	boost::filesystem::remove_all(vf);
	set_global(state, "vf_dir", vf.string());
	/* This encodes a DCP of 1 second, the OV */
	check_runs(state,
		"ov = dcpomatic.open_film(film_dir)\n"
		"ov:add_content(picture):set('video.length', dcpomatic.seconds(1))\n"
		"ov:add_content(wav)\n"
		"ov:make_dcp{ ignore_hints = true }\n"
		"vf = dcpomatic.new_film(vf_dir)\n"
		"dcp = vf:add_content(ov:dcp_folder())\n"
		"function set_all()\n"
		"  vf:set('video_frame_rate', ov:get('video_frame_rate'))\n"
		"  vf:set('video_bit_rate', ov:get('video_bit_rate'))\n"
		"  vf:set('container', ov:get('container'))\n"
		"  vf:set('resolution', ov:get('resolution'))\n"
		"  vf:set('three_d', false)\n"
		"  vf:set('audio_channels', ov:get('audio_channels'))\n"
		"  vf:set('audio_processor', nil)\n"
		"end\n"
		"set_all()\n"
		"dcp:set('dcp.reference_video', true)\n"
		"dcp:set('dcp.reference_audio', true)\n"
		);

	auto check_error = error_checker(state);
	auto const video = string("the film references a DCP's video, so it cannot change");
	auto const audio = string("the film references a DCP's audio, so it cannot change");
	check_error("vf:set('video_frame_rate', 25)", "property 'video_frame_rate': " + video);
	check_error("vf:set('video_bit_rate', 100000000)", "property 'video_bit_rate': " + video);
	check_error("vf:set('container', '239')", "property 'container': " + video);
	check_error("vf:set('resolution', '4K')", "property 'resolution': " + video);
	check_error("vf:set('three_d', true)", "property 'three_d': " + video);
	check_error("vf:set('audio_channels', 8)", "property 'audio_channels': " + audio);
	check_error("vf:set('audio_processor', 'stereo-5.1-upmix-a')", "property 'audio_processor': " + audio);
	check_error(
		"vf:set('interop', true)",
		"property 'interop': the DCP panel does not offer Interop: the film is SMPTE and references a DCP's video or audio"
		);
	check_runs(state, "vf:set('interop', false)\nassert(vf:get('video_frame_rate') == ov:get('video_frame_rate'))\n");
	/* Setting the value that is already there is no change, so it is allowed */
	check_runs(state, "set_all()\n");

	/* Without the references everything can change again */
	check_runs(state,
		"dcp:set('dcp.reference_video', false)\n"
		"dcp:set('dcp.reference_audio', false)\n"
		"set_all()\n"
		"vf:set('interop', true)\n"
		);
}


/** With MPEG2 the DCP panel disables the container, resolution and 3D, but not the frame rate or bit rate */
BOOST_AUTO_TEST_CASE(lua_gui_rules_mpeg2_picture)
{
	auto guard = standard_preferences(true, false);
	LuaState state;
	film_and_files(state, "lua_gui_rules_mpeg2_picture", {});
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:set('container', '185')\n"
		"film:set('resolution', '2K')\n"
		"film:set('three_d', false)\n"
		"film:set('interop', true)\n"
		"film:set('video_encoding', 'mpeg2')\n"
		"film:set('video_frame_rate', 25)\n"
		"film:set('video_bit_rate', 10000000)\n"
		);
	auto check_error = error_checker(state);
	auto const mpeg2 = string("the video encoding is MPEG2, so it cannot change");
	check_error("film:set('container', '239')", "property 'container': " + mpeg2);
	check_error("film:set('resolution', '4K')", "property 'resolution': " + mpeg2);
	check_error("film:set('three_d', true)", "property 'three_d': " + mpeg2);
	/* The values already there are no change */
	check_runs(state,
		"film:set('container', film:get('container'))\n"
		"film:set('resolution', film:get('resolution'))\n"
		"film:set('three_d', film:get('three_d'))\n"
		);
	/* Back to JPEG2000 they can change again */
	check_runs(state,
		"film:set('video_encoding', 'jpeg2000')\n"
		"film:set('container', '239')\n"
		"film:set('resolution', '4K')\n"
		"film:set('three_d', true)\n"
		"assert(film:get('container') == '239' and film:get('resolution') == '4K' and film:get('three_d') == true)\n"
		);
}


/** A film with Atmos content keeps its frame rate, as the DCP panel disables it */
BOOST_AUTO_TEST_CASE(lua_gui_rules_frame_rate_atmos)
{
	LuaState state;
	film_and_files(state, "lua_gui_rules_frame_rate_atmos", { { "atmos", "atmos_0.mxf" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:set('video_frame_rate', 25)\n"
		"film:set('video_frame_rate', 24)\n"
		"film:add_content(atmos)\n"
		);
	auto check_error = error_checker(state);
	check_error("film:set('video_frame_rate', 25)", "property 'video_frame_rate': the film has Atmos content, so it cannot change");
	/* The bit rate is not tied to the Atmos */
	check_runs(state, "film:set('video_bit_rate', 100000000)\n");
}


/** Turning use_isdcf_name back on cuts the name at the first _, as ticking Use ISDCF name does (#1513) */
BOOST_AUTO_TEST_CASE(lua_gui_rules_use_isdcf_name)
{
	LuaState state;
	film_and_files(state, "lua_gui_rules_use_isdcf_name", {});
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:set('use_isdcf_name', false)\n"
		"film:set('name', 'Jasmine_FTR-1_F_EN-XX')\n"
		"film:set('use_isdcf_name', true)\n"
		"assert(film:get('name') == 'Jasmine', film:get('name'))\n"
		/* Already on: the tick box cannot be ticked again, so the name stays */
		"film:set('name', 'Jasmine_Jambo')\n"
		"film:set('use_isdcf_name', true)\n"
		"assert(film:get('name') == 'Jasmine_Jambo', film:get('name'))\n"
		/* Turning it off leaves the name alone */
		"film:set('use_isdcf_name', false)\n"
		"assert(film:get('name') == 'Jasmine_Jambo', film:get('name'))\n"
		);
}

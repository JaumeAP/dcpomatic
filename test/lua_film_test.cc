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


/** @file  test/lua_film_test.cc
 *  @brief Tests of Lua scripting of film operations: markers, duplicates, templates, opening, disk space,
 *  frame rate and sending to a TMS.
 *  @ingroup feature
 */


#include "lib/config.h"
#include "lib/film.h"
#include "lua/lua_state.h"
#include "lua_test_helpers.h"
#include <dcp/scope_guard.h>
#include <dcp/types.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>
#include <string>


using std::make_shared;
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


/** film:markers(), film:set_marker() and film:unset_marker() read and change the markers as the Markers dialog does */
BOOST_AUTO_TEST_CASE(lua_film_markers)
{
	LuaState state;
	picture_and_sound(state, "lua_film_markers");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"assert(next(film:markers()) == nil)\n"
		"film:set_marker('FFOB', dcpomatic.seconds(1))\n"
		"film:set_marker('FFEC', dcpomatic.seconds(2))\n"
		"assert(film:markers().FFOB == dcpomatic.seconds(1), film:markers().FFOB)\n"
		"assert(math.type(film:markers().FFOB) == 'integer')\n"
		"assert(film:describe():find('\\nmarkers: {FFEC=192000, FFOB=96000}\\n', 1, true), film:describe())\n"
		"film:unset_marker('FFEC')\n"
		"assert(film:markers().FFEC == nil)\n"
		"dcpomatic.clear_warnings()\n"
		/* As the dialog, a time at or past the end is moved to the last frame */
		"film:set_marker('LFEC', dcpomatic.seconds(20))\n"
		"assert(film:markers().LFEC == film:length() - dcpomatic.frames(1, 24), film:markers().LFEC)\n"
		"assert(table.concat(dcpomatic.warnings(), '\\n'):find('LFEC', 1, true), table.concat(dcpomatic.warnings(), '\\n'))\n"
		"film:save()\n"
		);

	auto check_error = error_checker(state);
	check_error("film:set_marker('XXXX', 0)", "film:set_marker: argument 2: unknown marker 'XXXX'; the markers are FFOB, LFOB, FFTC, LFTC, FFOI, LFOI, FFEC, LFEC, FFMC, LFMC");
	check_error("film:unset_marker('XXXX')", "film:unset_marker: argument 2: unknown marker 'XXXX'; the markers are FFOC, LFOC, FFTC, LFTC, FFOI, LFOI, FFEC, LFEC, FFMC, LFMC, FFOB, LFOB");
	check_error("film:set_marker('FFOB', 0.5)", "film:set_marker: argument 3: expected integer ticks");
	check_error("film:set_marker('FFOB', -1)", "film:set_marker: argument 3: expected 0 or more ticks");
	check_error("film:set_marker(1, 0)", "film:set_marker: argument 2: expected string");
	/* The GUI offers neither of these: DCP-o-matic writes them */
	check_error(
		"film:set_marker('FFOC', 0)",
		"film:set_marker: argument 2: FFOC is set by DCP-o-matic itself, and the GUI does not offer to set it; "
		"the markers that can be set are FFOB, LFOB, FFTC, LFTC, FFOI, LFOI, FFEC, LFEC, FFMC, LFMC"
		);
	check_error("film:set_marker('LFOC', 0)", "film:set_marker: argument 2: LFOC is set by DCP-o-matic itself");
	/* but they can be removed, as the markers panel removes any marker it shows */
	check_runs(state, "film:unset_marker('FFOC')\nfilm:unset_marker('LFOC')\n");

	/* The DCP panel disables Markers for Interop and for SMPTE A */
	check_error(
		"film:set('interop', true)\nfilm:set_marker('FFOB', 0)",
		"film:set_marker: the film is Interop, whose DCPs have no markers (the GUI disables Markers then)"
		);
	check_runs(state, "film:set('interop', false)\nassert(film:markers().FFOB == dcpomatic.seconds(1))\n");
	/* SMPTE A is offered only when the preferences allow it */
	auto const allow_all = Config::instance()->allow_all_smpte_profiles();
	dcp::ScopeGuard sg = [allow_all]() { Config::instance()->set_allow_all_smpte_profiles(allow_all); };
	Config::instance()->set_allow_all_smpte_profiles(true);
	check_error(
		"film:set('smpte_profile', 'smpte-a')\nfilm:set_marker('FFOB', 0)",
		"film:set_marker: the film's SMPTE profile is SMPTE A, whose DCPs have no markers (the GUI disables Markers then)"
		);
	check_runs(state, "film:set('smpte_profile', 'smpte-bv21')\nassert(film:markers().FFOB == dcpomatic.seconds(1))\n");

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	film->read_metadata();
	BOOST_REQUIRE(film->marker(dcp::Marker::FFOB));
	BOOST_CHECK_EQUAL(film->marker(dcp::Marker::FFOB)->get(), 96000);
	BOOST_CHECK(!film->marker(dcp::Marker::FFEC));
	BOOST_REQUIRE(film->marker(dcp::Marker::LFEC));
	BOOST_CHECK_EQUAL(film->marker(dcp::Marker::LFEC)->get(), 10 * 96000 - 4000);
}


/** film:duplicate(dir) makes a copy of a saved film in dir, as File -> Duplicate does */
BOOST_AUTO_TEST_CASE(lua_film_duplicate)
{
	LuaState state;
	picture_and_sound(state, "lua_film_duplicate");
	auto const copy = test_film_dir("lua_film_duplicate_copy");
	boost::filesystem::remove_all(copy);
	boost::filesystem::remove_all(copy.string() + "_2");
	set_global(state, "copy_dir", copy.string());
	auto const file = test_film_dir("lua_film_duplicate_file");
	boost::filesystem::remove_all(file);
	boost::filesystem::create_directories(file.parent_path());
	write_wav(file, 1, 1);
	set_global(state, "file", file.string());
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		"film:add_content(wav)\n"
		"film:set('audio_channels', 12)\n"
		"film:save()\n"
		"local copy = film:duplicate(copy_dir)\n"
		"assert(copy:get('name') == 'lua_film_duplicate_copy', copy:get('name'))\n"
		"assert(copy:get('audio_channels') == 12)\n"
		"assert(#copy:content() == 2, #copy:content())\n"
		);

	auto check_error = error_checker(state);
	check_error("film:set('audio_channels', 8)\nfilm:duplicate(copy_dir .. '_2')", "film:duplicate: the film has unsaved changes; call film:save() first (the GUI offers to save them)");
	check_runs(state, "film:save()");
	/* As the GUI asks before it uses a directory that is not empty */
	check_error(
		"film:duplicate(copy_dir)",
		fmt::format("film:duplicate: the directory {} already exists and is not empty; pass {{ overwrite = true }} to use it", copy.string())
		);
	check_error("film:duplicate(copy_dir, { overwrit = true })", "film:duplicate: unknown option 'overwrit'");
	check_error(
		"film:duplicate(file)",
		fmt::format("film:duplicate: argument 2: {} already exists as a file, so you cannot use it for a film.", file.string())
		);
	check_error("film:duplicate(film_dir, { overwrite = true })", fmt::format("film:duplicate: argument 2: {} is the film's own directory", get_global(state, "film_dir")));
	check_error("film:duplicate(1)", "film:duplicate: argument 2: expected string");
	/* The saved film now has 8 channels, and the copy replaces the old one */
	check_runs(state, "assert(film:duplicate(copy_dir, { overwrite = true }):get('audio_channels') == 8)");

	auto film = make_shared<Film>(copy);
	film->read_metadata();
	BOOST_CHECK_EQUAL(film->name(), "lua_film_duplicate_copy");
	BOOST_CHECK_EQUAL(film->audio_channels(), 8);
	BOOST_CHECK_EQUAL(film->content().size(), 2U);
}


/** Templates are saved, listed, used, renamed and deleted as the GUI's Save as template, New and Templates dialogs do */
BOOST_AUTO_TEST_CASE(lua_film_templates)
{
	auto const config = test_film_dir("lua_film_templates_config");
	boost::filesystem::remove_all(config);
	boost::filesystem::create_directories(config);
	ConfigRestorer cr(config);

	LuaState state;
	picture_and_sound(state, "lua_film_templates");
	auto const from_template = test_film_dir("lua_film_templates_new");
	for (auto suffix: { "", "_2", "_default" }) {
		boost::filesystem::remove_all(from_template.string() + suffix);
	}
	set_global(state, "new_dir", from_template.string());
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"assert(#dcpomatic.templates() == 0)\n"
		"film:set('audio_channels', 12)\n"
		"film:save_as_template('Twelve')\n"
		"local names = dcpomatic.templates()\n"
		"assert(#names == 1 and names[1] == 'Twelve', table.concat(names, ','))\n"
		"local made = dcpomatic.new_film(new_dir, { template = 'Twelve' })\n"
		"assert(made:get('audio_channels') == 12, made:get('audio_channels'))\n"
		"dcpomatic.rename_template('Twelve', 'Doce')\n"
		"names = dcpomatic.templates()\n"
		"assert(#names == 1 and names[1] == 'Doce', table.concat(names, ','))\n"
		"film:set('audio_channels', 10)\n"
		"film:save_as_default_template()\n"
		"assert(dcpomatic.new_film(new_dir .. '_default'):get('audio_channels') == 10)\n"
		);

	auto check_error = error_checker(state);
	check_error("dcpomatic.new_film(new_dir .. '_2', { template = 'Twelve' })", "dcpomatic.new_film: option 'template': no template 'Twelve'; the templates are Doce");
	check_error("dcpomatic.new_film(new_dir .. '_2', { templat = 'Doce' })", "dcpomatic.new_film: unknown option 'templat'");
	check_error("dcpomatic.new_film(new_dir .. '_2', { template = 1 })", "dcpomatic.new_film: option 'template': expected string");
	check_error("film:save_as_template('Doce')", "film:save_as_template: there is already a template called 'Doce'; pass { overwrite = true } to replace it");
	check_runs(state, "film:save_as_template('Doce', { overwrite = true })");
	check_error("film:save_as_template('')", "film:save_as_template: argument 2: template names must not be empty");
	check_error("dcpomatic.rename_template('Doce', '')", "dcpomatic.rename_template: argument 2: template names must not be empty");
	check_error("dcpomatic.rename_template('Nope', 'Other')", "dcpomatic.rename_template: argument 1: no template 'Nope'; the templates are Doce");
	check_error("dcpomatic.delete_template('Nope')", "dcpomatic.delete_template: argument 1: no template 'Nope'; the templates are Doce");
	check_runs(state, "dcpomatic.delete_template('Doce')\nassert(#dcpomatic.templates() == 0)\n");

	auto film = make_shared<Film>(from_template);
	film->read_metadata();
	BOOST_CHECK_EQUAL(film->audio_channels(), 12);
}


/** dcpomatic.open_film checks the content as the GUI does when it loads a film: changed files are examined again,
 *  with the GUI's message as a warning, and missing files are a warning, not an error.
 */
BOOST_AUTO_TEST_CASE(lua_open_film_checks_content)
{
	DropJobsAtEnd drop;
	LuaState state;
	picture_and_sound(state, "lua_open_film_checks_content");
	check_runs(state,
		"local film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(wav)\n"
		"film:save()\n"
		);

	write_wav(get_global(state, "wav"), 1, 2);

	CerrCapture capture;
	check_runs(state,
		"dcpomatic.clear_warnings()\n"
		"local film = dcpomatic.open_film(film_dir)\n"
		"assert(film:content()[1]:full_length() == dcpomatic.seconds(2), film:content()[1]:full_length())\n"
		"warnings = table.concat(dcpomatic.warnings(), '\\n')\n"
		);
	auto const warnings = get_global(state, "warnings");
	/* The job keeps only its last message, and in a build older than 2.18.30 that is the one about a bug fix */
	BOOST_CHECK_MESSAGE(warnings.find("Checking content: Some files ") != string::npos, warnings);
	BOOST_CHECK_MESSAGE(capture.has_warning("Checking content: Some files "), capture.text());

	boost::filesystem::remove(get_global(state, "wav"));
	check_runs(state,
		"dcpomatic.clear_warnings()\n"
		"local film = dcpomatic.open_film(film_dir)\n"
		"assert(table.concat(film:content()[1]:status(), ',') == 'missing')\n"
		"missing = table.concat(dcpomatic.warnings(), '\\n')\n"
		);
	auto const missing = get_global(state, "missing");
	BOOST_CHECK_MESSAGE(missing.find("Checking content: ") != string::npos, missing);
}


/** film:make_dcp refuses, as the GUI asks first, when the disk may not have room for the DCP */
BOOST_AUTO_TEST_CASE(lua_make_dcp_disk_space)
{
	LuaState state;
	picture_and_sound(state, "lua_make_dcp_disk_space");
	/* 1000 hours of picture needs many terabytes */
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture):set('video.length', dcpomatic.seconds(1000 * 3600))\n"
		/* make_dcp names an unnamed film and dates an ISDCF name, either of which would change the folder */
		"film:set('use_isdcf_name', false)\n"
		"film:set('name', 'Disk')\n"
		"film:save()\n"
		"dcp_name = film:dcp_name()\n"
		);
	BOOST_REQUIRE_EQUAL(get_global(state, "dcp_name"), "Disk");

	/* An existing DCP folder stops make_dcp after the disk check and before it encodes anything, so this test
	 * cannot encode 1000 hours, even without the check.
	 */
	boost::filesystem::create_directories(boost::filesystem::path(get_global(state, "film_dir")) / get_global(state, "dcp_name"));

	auto check_error = error_checker(state);
	check_error("film:make_dcp{ ignore_hints = true }", "film:make_dcp: the DCP for this film will take up about ");
	check_error("film:make_dcp{ ignore_hints = true }", " GB available; pass { ignore_disk_space = true } to make it anyway");
	check_error("film:make_dcp{ ignore_hints = true, ignore_disk_space = 1 }", "option 'ignore_disk_space': expected boolean");
	check_error(
		"film:make_dcp{ ignore_hints = true, ignore_disk_space = true }",
		fmt::format(
			"the DCP folder {} already exists; pass {{ overwrite = true }} to replace it",
			(boost::filesystem::path(get_global(state, "film_dir")) / "Disk").string()
			)
		);
}


/** film:use_best_frame_rate() sets the frame rate that suits the content, as the DCP panel's Use best does */
BOOST_AUTO_TEST_CASE(lua_use_best_frame_rate)
{
	LuaState state;
	film_and_files(state, "lua_use_best_frame_rate", { { "red", "red_30.mp4" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(red)\n"
		"film:set('video_frame_rate', 24)\n"
		"assert(film:use_best_frame_rate() == 30)\n"
		"assert(film:get('video_frame_rate') == 30, film:get('video_frame_rate'))\n"
		);
}


/** film:use_best_frame_rate() is refused while the film references a DCP's video, as the DCP panel disables Use best */
BOOST_AUTO_TEST_CASE(lua_use_best_frame_rate_refused)
{
	LuaState state;
	picture_and_sound(state, "lua_use_best_frame_rate_refused");
	auto const vf = test_film_dir("lua_use_best_frame_rate_refused_vf");
	boost::filesystem::remove_all(vf);
	set_global(state, "vf_dir", vf.string());
	/* This encodes a DCP of 1 second, the OV */
	check_runs(state,
		"ov = dcpomatic.open_film(film_dir)\n"
		"ov:add_content(picture):set('video.length', dcpomatic.seconds(1))\n"
		"ov:make_dcp{ ignore_hints = true }\n"
		"vf = dcpomatic.new_film(vf_dir)\n"
		"vf:add_content(ov:dcp_folder()):set('dcp.reference_video', true)\n"
		);
	auto check_error = error_checker(state);
	check_error("vf:use_best_frame_rate()", "film:use_best_frame_rate: the film references a DCP's video, so its frame rate cannot change");
}


/** film:use_best_frame_rate() is refused for a film with Atmos content, as the DCP panel disables Use best */
BOOST_AUTO_TEST_CASE(lua_use_best_frame_rate_atmos)
{
	LuaState state;
	film_and_files(state, "lua_use_best_frame_rate_atmos", { { "atmos", "atmos_0.mxf" } });
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(atmos)\n"
		"assert(film:describe():find('\\nbest_video_frame_rate: ', 1, true), film:describe())\n"
		);
	auto check_error = error_checker(state);
	check_error("film:use_best_frame_rate()", "film:use_best_frame_rate: the film has Atmos content, so its frame rate cannot change");
}


/** film:send_to_tms() needs a DCP, as the menu item does, and reports the upload's error */
BOOST_AUTO_TEST_CASE(lua_send_to_tms)
{
	DropJobsAtEnd drop;
	LuaState state;
	picture_and_sound(state, "lua_send_to_tms");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture):set('video.length', dcpomatic.seconds(1))\n"
		);

	auto check_error = error_checker(state);
	check_error("film:send_to_tms()", "film:send_to_tms: the film has no DCP to send; make one first (the GUI offers Send DCP to TMS only then)");

	/* The test configuration has no TMS, so the upload fails; this encodes a DCP of 1 second */
	check_runs(state, "film:make_dcp{ ignore_hints = true }");
	check_error("film:send_to_tms()", "film:send_to_tms: Copy DCP to TMS: ");
}

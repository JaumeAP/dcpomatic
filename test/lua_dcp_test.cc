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


/** @file  test/lua_dcp_test.cc
 *  @brief Tests of Lua scripting of DCP content: VFs, OVs, CPLs, KDMs and references.
 *  @ingroup feature
 */


#include "lib/config.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lua/lua_state.h"
#include "lua_test_helpers.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/encrypted_kdm.h>
#include <dcp/local_time.h>
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>
#include <string>


using std::make_shared;
using std::string;


/** Make with Lua an OV of 1 second of picture and sound (global ov), or of 2 seconds with subtitles, and a new film
 *  vf_dir (global vf) with the OV as the content dcp.
 */
static void
ov_and_vf(LuaState& state, string const& name, bool subtitles = false)
{
	picture_and_sound(state, name);
	auto const vf = test_film_dir(name + "_vf");
	boost::filesystem::remove_all(vf);
	set_global(state, "vf_dir", vf.string());
	set_global(state, "srt", subtitles ? boost::filesystem::canonical("test/data/subrip2.srt").string() : "");
	check_runs(state,
		"ov = dcpomatic.open_film(film_dir)\n"
		"ov:add_content(picture):set('video.length', dcpomatic.seconds(srt == '' and 1 or 2))\n"
		"ov:add_content(wav)\n"
		"if srt ~= '' then ov:add_content(srt) end\n"
		"ov:make_dcp{ ignore_hints = true }\n"
		"vf = dcpomatic.new_film(vf_dir)\n"
		"dcp = vf:add_content(ov:dcp_folder())\n"
		);
}


/** @return the error of setting the reference row of dcp to true, which must leave the row false */
static string
reference_refused(LuaState& state, string const& row, string const& selector = "")
{
	auto const type = selector.empty() ? string() : ", '" + selector + "'";
	return error_from(state, fmt::format(
		"local ok, e = pcall(function() dcp:set('{0}', true{1}) end)\n"
		"assert(dcp:get('{0}'{1}) == false, 'the reference was set')\n"
		"error(ok and 'the reference was not refused' or e, 0)\n",
		row, type
		));
}


/** A VF references the video and audio of its OV as the DCP referencing dialog does, and a reference the dialog
 *  would not allow is refused with the library's reason and not set.
 */
BOOST_AUTO_TEST_CASE(lua_dcp_references)
{
	LuaState state;
	ov_and_vf(state, "lua_dcp_references", true);
	check_runs(state,
		"assert(dcp:get('dcp.reference_video') == false)\n"
		"dcp:set('dcp.reference_video', true)\n"
		"dcp:set('dcp.reference_audio', true)\n"
		"dcp:set('dcp.reference_text', true, 'open-subtitle')\n"
		"assert(dcp:get('dcp.reference_video') == true)\n"
		"assert(dcp:get('dcp.reference_audio') == true)\n"
		"assert(dcp:get('dcp.reference_text', 'open-subtitle') == true)\n"
		"assert(dcp:get('dcp.reference_text') == true)\n"
		"vf:make_dcp{ ignore_hints = true }\n"
		"vf_folder = vf:dcp_folder()\n"
		"local text = vf:describe()\n"
		"assert(text:find('\\ndcp.reference_video: true\\n', 1, true), text)\n"
		"assert(text:find('\\ndcp.reference_text (open-subtitle): true\\n', 1, true), text)\n"
		"assert(text:find('\\ndcp.reference_text (closed-caption): false\\n', 1, true), text)\n"
		"assert(text:find('\\ncpls: {id=', 1, true), text)\n"
		);

	/* The VF's DCP has no picture, sound or subtitles of its own */
	int mxfs = 0;
	for (auto const& file: boost::filesystem::directory_iterator(get_global(state, "vf_folder"))) {
		if (file.path().extension() == ".mxf") {
			++mxfs;
		}
	}
	BOOST_CHECK_EQUAL(mxfs, 0);

	auto check_error = [](string const& error, string const& expected) {
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_runs(state, "dcp:set('dcp.reference_video', false)\ndcp:set('dcp.reference_audio', false)\ndcp:set('dcp.reference_text', false)\n");

	check_runs(state, "vf:set('resolution', '4K')");
	check_error(reference_refused(state, "dcp.reference_video"), "property 'dcp.reference_video': cannot reference this DCP's video: it is 2K and the film is 4K.");
	check_runs(state, "vf:set('resolution', '2K')");

	check_runs(state, "vf:set('audio_channels', 8)");
	check_error(reference_refused(state, "dcp.reference_audio"), "property 'dcp.reference_audio': cannot reference this DCP's audio: it has a different number of audio channels than the project");
	check_runs(state, "vf:set('audio_channels', ov:get('audio_channels'))");

	check_runs(state, "vf:set('interop', true)");
	check_error(reference_refused(state, "dcp.reference_video"), "property 'dcp.reference_video': cannot reference this DCP: it is SMPTE and the film is set to Interop.");
	check_runs(state, "vf:set('interop', false)");

	check_error(reference_refused(state, "dcp.reference_atmos"), "property 'dcp.reference_atmos': cannot reference this DCP's Atmos: it has no Atmos track.");

	/* The library counts a text that the DCP does not have as overlapping, so the dialog refuses it too */
	check_error(reference_refused(state, "dcp.reference_text", "closed-caption"), "property 'dcp.reference_text': cannot reference this DCP's closed captions: it overlaps other text content.");

	check_runs(state, "dcp:set('trim_start', 4000)");
	check_error(
		reference_refused(state, "dcp.reference_text", "open-subtitle"),
		"property 'dcp.reference_text': cannot reference this DCP's subtitles: it has a start trim so its subtitles or closed captions must be re-written."
		);
	check_runs(state, "dcp:set('trim_start', 0)");

	check_error(error_from(state, "dcp:set('dcp.reference_text', true, 'open-caption')"), "property 'dcp.reference_text': the GUI references only open-subtitle and closed-caption texts");
	check_error(error_from(state, "dcp:set('dcp.reference_video', true, 'open-subtitle')"), "property 'dcp.reference_video': only text.* properties and dcp.reference_text take a text type");
	check_error(error_from(state, "dcp:set('dcp.reference_video', 1)"), "property 'dcp.reference_video': expected boolean");
	check_error(error_from(state, "vf:add_content(picture):set('dcp.reference_video', true)"), "property 'dcp.reference_video': the content is not a DCP");
}


/** c:cpls() lists the CPLs as the Choose CPL menu does, dcp.cpl chooses one, and c:add_ov() gives a VF its OV */
BOOST_AUTO_TEST_CASE(lua_dcp_cpl_and_ov)
{
	LuaState state;
	ov_and_vf(state, "lua_dcp_cpl_and_ov");
	auto const third = test_film_dir("lua_dcp_cpl_and_ov_third");
	boost::filesystem::remove_all(third);
	set_global(state, "third_dir", third.string());
	check_runs(state,
		"dcp:set('dcp.reference_video', true)\n"
		"dcp:set('dcp.reference_audio', true)\n"
		"vf:make_dcp{ ignore_hints = true }\n"
		"local cpls = dcp:cpls()\n"
		"assert(#cpls == 1, #cpls)\n"
		"assert(cpls[1].id == dcp:get('dcp.cpl'), cpls[1].id)\n"
		"assert(cpls[1].title == ov:dcp_name(), cpls[1].title)\n"
		"dcp:set('dcp.cpl', cpls[1].id)\n"
		"third = dcpomatic.new_film(third_dir)\n"
		"v = third:add_content(vf:dcp_folder())\n"
		"assert(table.concat(v:status(), ',') == 'needs OV', table.concat(v:status(), ','))\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("v:add_ov('/nonexistent/ov')", "c:add_ov: argument 2: /nonexistent/ov is not a folder");
	check_runs(state,
		"v:add_ov(ov:dcp_folder())\n"
		"assert(#v:status() == 0, table.concat(v:status(), ','))\n"
		"local cpls = v:cpls()\n"
		"assert(#cpls == 2, #cpls)\n"
		"other = cpls[1].id == v:get('dcp.cpl') and cpls[2].id or cpls[1].id\n"
		"v:set('dcp.cpl', other)\n"
		"assert(v:get('dcp.cpl') == other)\n"
		"third:save()\n"
		);
	check_error("v:add_ov(ov:dcp_folder())", "c:add_ov: the DCP does not need an OV (the GUI offers Add OV only when it does)");
	check_error("v:set('dcp.cpl', 'nonsense')", "property 'dcp.cpl': no CPL 'nonsense'; the CPLs are ");
	check_error("v:set('dcp.cpl', 1)", "property 'dcp.cpl': expected string");
	check_error("v:get('dcp.reference_text')", "property 'dcp.reference_text': name the text type, 'open-subtitle' or 'closed-caption'");
	check_runs(state, "w = third:add_content(wav)");
	check_error("w:cpls()", "c:cpls: the content is not a DCP");
	check_error("w:add_ov(ov:dcp_folder())", "c:add_ov: the content is not a DCP");
	check_error("w:get('dcp.cpl')", "property 'dcp.cpl': the content is not a DCP");

	auto film = make_shared<Film>(third);
	film->read_metadata();
	BOOST_REQUIRE(!film->content().empty());
	auto dcp = std::dynamic_pointer_cast<DCPContent>(film->content()[0]);
	BOOST_REQUIRE(dcp);
	BOOST_CHECK(dcp->cpl() == get_global(state, "other"));
}


/** c:add_kdm() decrypts an encrypted DCP as the content menu's Add KDM does */
BOOST_AUTO_TEST_CASE(lua_dcp_kdm)
{
	LuaState state;
	picture_and_sound(state, "lua_dcp_kdm");
	auto const vf = test_film_dir("lua_dcp_kdm_vf");
	boost::filesystem::remove_all(vf);
	set_global(state, "vf_dir", vf.string());
	set_global(state, "plain", boost::filesystem::canonical("test/data/scaling_test_133_185").string());
	check_runs(state,
		"ov = dcpomatic.open_film(film_dir)\n"
		"ov:add_content(picture):set('video.length', dcpomatic.seconds(1))\n"
		"ov:set('encrypt_picture', true)\n"
		"ov:make_dcp{ ignore_hints = true }\n"
		"ov_folder = ov:dcp_folder()\n"
		);

	auto const ov_folder = boost::filesystem::path(get_global(state, "ov_folder"));
	dcp::DCP ov_dcp(ov_folder);
	ov_dcp.read();
	BOOST_REQUIRE_EQUAL(ov_dcp.cpls().size(), 1U);
	auto ov = make_shared<Film>(boost::filesystem::path(get_global(state, "film_dir")));
	ov->read_metadata();
	auto const decrypted = ov->make_kdm(ov_dcp.cpls()[0]->file().get(), dcp::LocalTime("2030-07-21T00:00:00+00:00"), dcp::LocalTime("2031-07-21T00:00:00+00:00"));
	auto const kdm = decrypted.encrypt(Config::instance()->signer_chain(), Config::instance()->decryption_chain()->leaf(), {}, dcp::Formulation::MODIFIED_TRANSITIONAL_1, true, 0);
	auto const kdm_file = test_film_dir("lua_dcp_kdm") / "kdm.xml";
	kdm.as_xml(kdm_file);
	set_global(state, "kdm", kdm_file.string());

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_runs(state,
		"vf = dcpomatic.new_film(vf_dir)\n"
		"dcp = vf:add_content(ov_folder)\n"
		"assert(table.concat(dcp:status(), ',') == 'needs KDM', table.concat(dcp:status(), ','))\n"
		);
	check_error("dcp:add_kdm('/nonexistent/kdm.xml')", "c:add_kdm: could not load KDM: ");
	check_error("dcp:add_kdm(picture)", "c:add_kdm: could not load KDM: ");
	check_runs(state,
		"dcp:add_kdm(kdm)\n"
		"assert(#dcp:status() == 0, table.concat(dcp:status(), ','))\n"
		"vf:save()\n"
		);
	check_error("vf:add_content(plain):add_kdm(kdm)", "c:add_kdm: the DCP is not encrypted (the GUI offers Add KDM only for one that is)");
	check_error("vf:add_content(wav):add_kdm(kdm)", "c:add_kdm: the content is not a DCP");

	auto film = make_shared<Film>(vf);
	film->read_metadata();
	BOOST_REQUIRE(!film->content().empty());
	auto dcp = std::dynamic_pointer_cast<DCPContent>(film->content()[0]);
	BOOST_REQUIRE(dcp);
	BOOST_CHECK(dcp->kdm());
}


/** film:copy_dcp_settings_from() and film:copy_dcp_markers_from() do what the content menu's items of the same names do */
BOOST_AUTO_TEST_CASE(lua_copy_dcp_settings_and_markers)
{
	LuaState state;
	ov_and_vf(state, "lua_copy_dcp_settings_and_markers");
	check_runs(state,
		"vf:set('resolution', '4K')\n"
		"vf:set('audio_channels', 8)\n"
		"vf:set('reel_type', 'single')\n"
		"vf:copy_dcp_settings_from(dcp)\n"
		"assert(vf:get('resolution') == '2K', vf:get('resolution'))\n"
		"assert(vf:get('audio_channels') == ov:get('audio_channels'), vf:get('audio_channels'))\n"
		"assert(vf:get('reel_type') == 'by-video-content', vf:get('reel_type'))\n"
		"assert(vf:get('name') == ov:dcp_name():match('^[^_]*'), vf:get('name'))\n"
		"vf:copy_dcp_markers_from(dcp)\n"
		"vf:save()\n"
		);

	auto check_error = [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};

	check_error("vf:copy_dcp_settings_from(vf:add_content(wav))", "film:copy_dcp_settings_from: argument 2: the content is not a DCP");
	check_error("vf:copy_dcp_markers_from(vf:add_content(wav))", "film:copy_dcp_markers_from: argument 2: the content is not a DCP");
	check_error("vf:copy_dcp_settings_from(1)", "film:copy_dcp_settings_from: argument 2: expected a Content");
	check_error("ov:copy_dcp_markers_from(dcp)", "film:copy_dcp_markers_from: argument 2: the content is in another film");

	auto film = make_shared<Film>(boost::filesystem::path(get_global(state, "vf_dir")));
	film->read_metadata();
	BOOST_CHECK(film->marker(dcp::Marker::FFOC));
	BOOST_CHECK(film->marker(dcp::Marker::LFOC));
}


/** The Atmos of a two-reel DCP is referenced only when the film's reels include the DCP's, as the dialog says */
BOOST_AUTO_TEST_CASE(lua_dcp_reference_atmos)
{
	/* A DCP of test/data/atmos_0.mxf with two reels, split at 2s, and Atmos in both of them */
	auto ov = new_test_film("lua_dcp_reference_atmos_ov", content_factory("test/data/atmos_0.mxf"));
	ov->set_reel_type(ReelType::CUSTOM);
	ov->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_seconds(2) });
	make_and_verify_dcp(ov, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });
	auto const atmos = boost::filesystem::canonical(ov->dir(ov->dcp_name()));

	LuaState state;
	auto const dir = test_film_dir("lua_dcp_reference_atmos");
	boost::filesystem::remove_all(dir);
	set_global(state, "film_dir", dir.string());
	set_global(state, "atmos", atmos.string());
	check_runs(state,
		"film = dcpomatic.new_film(film_dir)\n"
		"dcp = film:add_content(atmos)\n"
		"film:set('reel_type', 'single')\n"
		);
	auto const error = reference_refused(state, "dcp.reference_atmos");
	BOOST_CHECK_MESSAGE(
		error.find(
			"property 'dcp.reference_atmos': cannot reference this DCP: its reel start and end points are not included in the film's list; "
			"set the reel mode to 'split by video content'."
			) != string::npos,
		error
		);
	check_runs(state,
		"film:set('reel_type', 'custom')\n"
		"film:set('custom_reel_boundaries', { dcpomatic.seconds(2) })\n"
		"dcp:set('dcp.reference_atmos', true)\n"
		"assert(dcp:get('dcp.reference_atmos') == true)\n"
		"film:save()\n"
		);

	auto film = make_shared<Film>(dir);
	film->read_metadata();
	BOOST_REQUIRE_EQUAL(film->content().size(), 1U);
	auto dcp = std::dynamic_pointer_cast<DCPContent>(film->content()[0]);
	BOOST_REQUIRE(dcp);
	BOOST_CHECK(dcp->reference_atmos());
}

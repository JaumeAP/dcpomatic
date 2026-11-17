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


/** @file  test/lua_export_test.cc
 *  @brief Tests of Lua scripting of the export of video, audio and subtitle files.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/ffmpeg_audio_stream.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_examiner.h"
#include "lua/lua_state.h"
#include "lua_test_helpers.h"
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>
#include <string>


using std::dynamic_pointer_cast;
using std::string;


static std::function<void (string const&, string const&)>
error_checker(LuaState& state)
{
	return [&state](string const& code, string const& expected) {
		auto const error = error_from(state, code);
		BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
	};
}


/** Make the globals of picture_and_sound() and out_dir, an empty folder for what is exported, as an absolute path as the export dialogs need */
static void
export_test_setup(LuaState& state, string const& name)
{
	picture_and_sound(state, name);
	auto const out = boost::filesystem::absolute(test_film_dir(name + "_out"));
	boost::filesystem::remove_all(out);
	boost::filesystem::create_directories(out);
	set_global(state, "out_dir", out.string());
}


static std::shared_ptr<FFmpegExaminer>
examine(boost::filesystem::path path)
{
	auto content = dynamic_pointer_cast<FFmpegContent>(content_factory(path)[0]);
	BOOST_REQUIRE(content);
	return std::make_shared<FFmpegExaminer>(content);
}


/** film:export{} writes the files that Export video file and Export audio file write, with the options the dialogs offer.
 *  The film is 2 seconds of a still at 24fps with 1 second of sound, so little is written.
 */
BOOST_AUTO_TEST_CASE(lua_export_video_and_audio)
{
	DropJobsAtEnd drop;
	LuaState state;
	export_test_setup(state, "lua_export_video_and_audio");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture):set('video.length', dcpomatic.seconds(2))\n"
		"film:add_content(wav)\n"
		"channels = tostring(film:get('audio_channels'))\n"
		/* As the dialogs, the extension is the format's */
		"function one(files) assert(#files == 1, #files) return files[1] end\n"
		"mp4 = one(film:export{ file = out_dir .. '/video', format = 'mp4', mixdown = 'stereo', crf = 40 })\n"
		"assert(mp4 == out_dir .. '/video.mp4', mp4)\n"
		"streams = one(film:export{ file = out_dir .. '/streams.mov', format = 'mp4', split_streams = true })\n"
		"assert(streams == out_dir .. '/streams.mp4', streams)\n"
		"all = one(film:export{ file = out_dir .. '/all.wav', format = 'wav-24' })\n"
		"stereo = one(film:export{ file = out_dir .. '/stereo', format = 'wav-16', mixdown = 'none' })\n"
		"stereo = one(film:export{ file = out_dir .. '/stereo', format = 'wav-16', mixdown = 'stereo', overwrite = true })\n"
		);
	auto const channels = std::stoi(get_global(state, "channels"));
	BOOST_REQUIRE(channels > 2);

	auto mp4 = examine(get_global(state, "mp4"));
	BOOST_CHECK(mp4->has_video());
	BOOST_CHECK_EQUAL(mp4->video_length(), 48);
	BOOST_REQUIRE_EQUAL(mp4->audio_streams().size(), 1U);
	BOOST_CHECK_EQUAL(mp4->audio_streams()[0]->channels(), 2);

	auto streams = examine(get_global(state, "streams"));
	BOOST_CHECK_EQUAL(streams->video_length(), 48);
	BOOST_REQUIRE_EQUAL(streams->audio_streams().size(), static_cast<size_t>(channels));
	BOOST_CHECK_EQUAL(streams->audio_streams()[0]->channels(), 1);

	auto all = examine(get_global(state, "all"));
	BOOST_CHECK(!all->has_video());
	BOOST_REQUIRE_EQUAL(all->audio_streams().size(), 1U);
	BOOST_CHECK_EQUAL(all->audio_streams()[0]->channels(), channels);
	BOOST_CHECK_EQUAL(all->audio_streams()[0]->frame_rate(), 48000);
	BOOST_CHECK_EQUAL(all->audio_streams()[0]->length(), 2 * 48000);
	BOOST_CHECK_EQUAL(all->audio_streams()[0]->bit_depth().get_value_or(0), 24);

	auto stereo = examine(get_global(state, "stereo"));
	BOOST_REQUIRE_EQUAL(stereo->audio_streams().size(), 1U);
	BOOST_CHECK_EQUAL(stereo->audio_streams()[0]->channels(), 2);
	BOOST_CHECK_EQUAL(stereo->audio_streams()[0]->bit_depth().get_value_or(0), 16);
}


/** film:export{ split_reels = true } writes a file per reel, returns their names, and asks before overwriting any of them */
BOOST_AUTO_TEST_CASE(lua_export_split_reels)
{
	DropJobsAtEnd drop;
	LuaState state;
	export_test_setup(state, "lua_export_split_reels");
	/* Two 1-second stills make two reels; the export is audio only, so no picture is encoded */
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture):set('video.length', dcpomatic.seconds(1))\n"
		"film:add_content(picture):set('video.length', dcpomatic.seconds(1))\n"
		"film:add_content(wav)\n"
		"film:set('reel_type', 'by-video-content')\n"
		"assert(#film:reels() == 2, #film:reels())\n"
		"files = film:export{ file = out_dir .. '/reels', format = 'wav-16', split_reels = true }\n"
		"assert(#files == 2, #files)\n"
		"assert(files[1] == out_dir .. '/reels_reel1.wav', files[1])\n"
		"assert(files[2] == out_dir .. '/reels_reel2.wav', files[2])\n"
		);
	auto const out = boost::filesystem::absolute(test_film_dir("lua_export_split_reels_out"));
	BOOST_CHECK(!boost::filesystem::exists(out / "reels.wav"));
	for (auto reel: { "reels_reel1.wav", "reels_reel2.wav" }) {
		auto examiner = examine(out / reel);
		BOOST_REQUIRE_EQUAL(examiner->audio_streams().size(), 1U);
		BOOST_CHECK_EQUAL(examiner->audio_streams()[0]->length(), 48000);
	}

	auto check_error = error_checker(state);
	check_error(
		"film:export{ file = out_dir .. '/reels', format = 'wav-16', split_reels = true }",
		fmt::format("film:export: {} already exists; pass {{ overwrite = true }} to replace it", (out / "reels_reel1.wav").string())
		);
}


/** film:export{} refuses what the dialogs do not offer, and writes nothing then */
BOOST_AUTO_TEST_CASE(lua_export_refused)
{
	DropJobsAtEnd drop;
	LuaState state;
	export_test_setup(state, "lua_export_refused");
	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(wav)\n"
		"out = out_dir .. '/out.wav'\n"
		);
	auto check_error = error_checker(state);
	check_error(
		"film:export{ file = out, format = 'wav-16', quality = 3 }",
		"film:export: unknown option 'quality'; the options are file, format, mixdown, split_reels, split_streams, crf, overwrite"
		);
	check_error(
		"film:export{ file = out, format = 'avi' }",
		"film:export: unknown format 'avi'; the formats are prores-4444, prores-hq, prores-lt, mp4, wav-16, wav-24"
		);
	check_error("film:export{ file = out }", "film:export: option 'format' is needed; the formats are prores-4444, prores-hq, prores-lt, mp4, wav-16, wav-24");
	check_error("film:export{ format = 'wav-16' }", "film:export: option 'file' is needed");
	check_error("film:export(out)", "film:export: argument 2: expected a table of options");
	/* The dialogs enable OK only for an absolute path */
	check_error("film:export{ file = 'lua_export_refused_relative.wav', format = 'wav-16' }", "film:export: option 'file': lua_export_refused_relative.wav is not an absolute path");
	BOOST_CHECK(!boost::filesystem::exists("lua_export_refused_relative.wav"));
	check_error("film:export{ file = out, format = 'wav-16', mixdown = '5.1' }", "film:export: unknown mixdown '5.1'; the mixdowns are none, stereo");
	check_error("film:export{ file = out, format = 'wav-16', mixdown = true }", "film:export: option 'mixdown': expected string");
	check_error("film:export{ file = out, format = 'wav-16', crf = 20 }", "film:export: option 'crf' is only for format mp4");
	check_error("film:export{ file = out, format = 'prores-hq', crf = 20 }", "film:export: option 'crf' is only for format mp4");
	check_error("film:export{ file = out, format = 'mp4', crf = 52 }", "film:export: option 'crf' must be from 0 (best) to 51 (worst)");
	check_error("film:export{ file = out, format = 'mp4', crf = 'good' }", "film:export: option 'crf': expected integer");
	check_error("film:export{ file = out, format = 'wav-24', split_streams = true }", "film:export: option 'split_streams' is only for the video formats");
	BOOST_CHECK(!boost::filesystem::exists(get_global(state, "out")));
	BOOST_CHECK(!boost::filesystem::exists(test_film_dir("lua_export_refused_out") / "out.mp4"));

	/* As the GUI asks before it overwrites */
	check_runs(state, "film:export{ file = out, format = 'wav-16' }");
	check_error(
		"film:export{ file = out, format = 'wav-16' }",
		fmt::format("film:export: {} already exists; pass {{ overwrite = true }} to replace it", get_global(state, "out"))
		);
}


static string
file_text(boost::filesystem::path path)
{
	std::ifstream file(path.string());
	return string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}


/** film:export_subtitles{} writes the files that Export subtitles writes, with the dialog's options */
BOOST_AUTO_TEST_CASE(lua_export_subtitles)
{
	DropJobsAtEnd drop;
	LuaState state;
	export_test_setup(state, "lua_export_subtitles");
	set_global(state, "srt", boost::filesystem::canonical("test/data/dcp_sub3.srt").string());
	auto const out = boost::filesystem::absolute(test_film_dir("lua_export_subtitles_out"));
	auto const xml_dir = out / "xml";
	boost::filesystem::create_directories(xml_dir);
	set_global(state, "xml_dir", xml_dir.string());
	auto check_error = error_checker(state);

	check_runs(state,
		"film = dcpomatic.open_film(film_dir)\n"
		"film:add_content(picture)\n"
		);
	check_error("film:export_subtitles{ file = out_dir .. '/subs' }", "film:export_subtitles: Your project contains no open subtitles, so there is nothing to export.");
	check_runs(state, "subs = film:add_content(srt)\nsubs:set('text.use', false)\n");
	check_error(
		"film:export_subtitles{ file = out_dir .. '/subs' }",
		"film:export_subtitles: Your project contains open subtitles, but none are being used.  Tick the \"Use as\" checkbox for the subtitles that you want to use."
		);

	check_runs(state,
		"subs:set('text.use', true)\n"
		"srt_out = film:export_subtitles{ file = out_dir .. '/subs.txt', format = 'srt' }\n"
		"assert(srt_out == out_dir .. '/subs.srt', srt_out)\n"
		/* The dialog's first choice: MXF for SMPTE */
		"mxf_out = film:export_subtitles{ file = out_dir .. '/subs' }\n"
		"assert(mxf_out == out_dir .. '/subs.mxf', mxf_out)\n"
		/* An Interop XML with its font is more than one file, so it goes into a folder */
		"assert(film:export_subtitles{ dir = xml_dir, format = 'xml', include_font = true } == xml_dir)\n"
		);
	BOOST_CHECK(file_text(get_global(state, "srt_out")).find("00:00:00,833 --> 00:00:06,125\nThis<i> is </i>wrong.") != string::npos);
	BOOST_CHECK(boost::filesystem::file_size(get_global(state, "mxf_out")) > 0);
	/* The XML is named after the film, in the folder; this subtitle has no font of its own, so no font file is written */
	int xml = 0;
	for (auto const& entry: boost::filesystem::directory_iterator(get_global(state, "xml_dir"))) {
		BOOST_CHECK_EQUAL(entry.path().extension().string(), ".xml");
		BOOST_CHECK(file_text(entry.path()).find("<DCSubtitle") != string::npos);
		BOOST_CHECK(file_text(entry.path()).find("wrong.") != string::npos);
		++xml;
		/* The file in the folder is checked before it is overwritten, too */
		check_error(
			"film:export_subtitles{ dir = xml_dir, format = 'xml', include_font = true }",
			fmt::format("film:export_subtitles: {} already exists; pass {{ overwrite = true }} to replace it", entry.path().string())
			);
	}
	BOOST_CHECK_EQUAL(xml, 1);
	check_runs(state, "film:export_subtitles{ dir = xml_dir, format = 'xml', include_font = true, overwrite = true }");

	/* The dialog enables OK only for an absolute path */
	check_error("film:export_subtitles{ file = 'lua_export_subtitles_relative', format = 'srt' }", "film:export_subtitles: option 'file': lua_export_subtitles_relative is not an absolute path");
	check_error("film:export_subtitles{ dir = 'build', format = 'xml', include_font = true }", "film:export_subtitles: option 'dir': build is not an absolute path");
	BOOST_CHECK(!boost::filesystem::exists("lua_export_subtitles_relative.srt"));

	check_error(
		"film:export_subtitles{ file = out_dir .. '/a', format = 'ass' }",
		"film:export_subtitles: unknown format 'ass'; the formats are xml, mxf, srt"
		);
	check_error(
		"film:export_subtitles{ file = out_dir .. '/a', fonts = true }",
		"film:export_subtitles: unknown option 'fonts'; the options are file, dir, format, split_reels, include_font, overwrite"
		);
	/* As the dialog's file picker asks before it overwrites */
	check_error(
		"film:export_subtitles{ file = out_dir .. '/subs', format = 'srt' }",
		fmt::format("film:export_subtitles: {} already exists; pass {{ overwrite = true }} to replace it", get_global(state, "srt_out"))
		);
	check_runs(state, "film:export_subtitles{ file = out_dir .. '/subs', format = 'srt', overwrite = true }");
	check_error(
		"film:export_subtitles{ file = out_dir .. '/a', format = 'srt', include_font = true }",
		"film:export_subtitles: option 'include_font' is only for format xml (MXF always includes the font)"
		);
	check_error(
		"film:export_subtitles{ file = out_dir .. '/a', format = 'xml', include_font = true }",
		"film:export_subtitles: this export writes more than one file, so give a folder as option 'dir', not option 'file'"
		);
	check_error(
		"film:export_subtitles{ dir = out_dir, format = 'srt' }",
		"film:export_subtitles: this export writes one file, so give it as option 'file', not option 'dir'"
		);
	check_error(
		"film:export_subtitles{ dir = out_dir .. '/missing', split_reels = true }",
		"film:export_subtitles: option 'dir': " + (out / "missing").string() + " is not a folder"
		);
	BOOST_CHECK(!boost::filesystem::exists(out / "a.srt"));
	BOOST_CHECK(!boost::filesystem::exists(out / "a.xml"));

	/* The dialog greys out splitting into reels when the film has more than one */
	check_runs(state,
		"film:add_content(picture)\n"
		"film:set('reel_type', 'by-video-content')\n"
		"assert(#film:reels() == 2, #film:reels())\n"
		);
	check_error(
		"film:export_subtitles{ dir = out_dir, split_reels = true }",
		"film:export_subtitles: option 'split_reels' is not offered for a film with more than one reel (the GUI greys it out)"
		);
}

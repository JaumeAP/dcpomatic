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


/** @file  test/lua_audio_test.cc
 *  @brief Tests of Lua scripting of the audio analysis and the gain calculator.
 *  @ingroup feature
 */


#include "lib/cinema_sound_processor.h"
#include "lua/lua_state.h"
#include "lua_test_helpers.h"
#include <boost/test/unit_test.hpp>
#include <cmath>
#include <fstream>
#include <string>


using std::string;


/** Write a 1-second, 48kHz, 16-bit stereo WAV of a 1kHz sine, peaking at half of full scale (-6.02dBFS) on the
 *  left and a quarter (-12.04dBFS) on the right.  At 48 samples to a cycle the peak falls exactly on a sample.
 */
static void
write_sine_wav(boost::filesystem::path path)
{
	int const rate = 48000;
	int const channels = 2;
	int const data_size = rate * channels * 2;

	std::ofstream file(path.string(), std::ios::binary);
	auto u16 = [&file](uint16_t v) { file.put(v & 0xff); file.put(v >> 8); };
	auto u32 = [&u16](uint32_t v) { u16(v & 0xffff); u16(v >> 16); };

	file.write("RIFF", 4);
	u32(4 + 8 + 16 + 8 + data_size);
	file.write("WAVEfmt ", 8);
	u32(16);
	u16(1);
	u16(channels);
	u32(rate);
	u32(rate * channels * 2);
	u16(channels * 2);
	u16(16);
	file.write("data", 4);
	u32(data_size);
	for (int i = 0; i < rate; ++i) {
		auto const s = std::sin(2 * M_PI * 1000 * i / rate);
		u16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(16384 * s))));
		u16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(8192 * s))));
	}
}


/** Check that code raises an error whose message contains expected */
static void
check_error(LuaState& state, string const& code, string const& expected)
{
	auto const error = error_from(state, code);
	BOOST_CHECK_MESSAGE(error.find(expected) != string::npos, error);
}


/** Make a saved, empty film and the sine WAV, as the globals film_dir and wav */
static void
film_and_sine(LuaState& state, string const& name)
{
	auto film = new_test_film(name);
	auto const wav = test_film_dir(name + "_content") / "sine.wav";
	boost::filesystem::remove_all(wav.parent_path());
	boost::filesystem::create_directories(wav.parent_path());
	write_sine_wav(wav);
	set_global(state, "film_dir", film->directory()->string());
	set_global(state, "wav", wav.string());
}


/** c:audio_analysis() and film:audio_analysis() run the audio analysis and give the levels the audio dialog shows,
 *  in dB with the content's gain applied as the dialog applies it.
 */
BOOST_AUTO_TEST_CASE(lua_audio_analysis)
{
	LuaState state;
	film_and_sine(state, "lua_audio_analysis");
	/* The true peak and loudness come from FFmpeg's ebur128 filter, which only a patched FFmpeg has */
#ifdef DCPOMATIC_HAVE_EBUR128_PATCHED_FFMPEG
	set_global(state, "ebur128", "yes");
#else
	set_global(state, "ebur128", "no");
	skipping("lua_audio_analysis: no true peak or loudness to check, as FFmpeg is not patched for ebur128");
#endif
	check_runs(state,
		"function near(a, b) return math.abs(a - b) < 0.05 end\n"
		"film = dcpomatic.open_film(film_dir)\n"
		"c = film:add_content(wav)\n"
		"assert(film:describe():find('\\naudio peak: not analysed yet (c:audio_analysis() analyses it)\\n', 1, true), film:describe())\n"
		"assert(film:describe():find('\\naudio peak: not analysed yet (film:audio_analysis() analyses it)\\n', 1, true), film:describe())\n"
		"a = c:audio_analysis()\n"
		"assert(near(a.peak_db, -6.02), a.peak_db)\n"
		"assert(math.type(a.peak_time) == 'integer', a.peak_time)\n"
		"assert(a.peak_time >= 0 and a.peak_time < dcpomatic.seconds(1), a.peak_time)\n"
		"assert(a.peak_channel == 'L', a.peak_channel)\n"
		"if ebur128 == 'yes' then\n"
		/* The true peak of a sampled sine is at least its sample peak */
		"  assert(a.true_peak_db >= a.peak_db - 0.01 and a.true_peak_db < -5.5, a.true_peak_db)\n"
		"  assert(a.channels[2].true_peak_db >= a.channels[2].peak_db - 0.01, a.channels[2].true_peak_db)\n"
		"  assert(type(a.integrated_loudness) == 'number', a.integrated_loudness)\n"
		"  assert(type(a.loudness_range) == 'number', a.loudness_range)\n"
		"else\n"
		"  assert(a.true_peak_db == nil and a.channels[1].true_peak_db == nil and a.integrated_loudness == nil and a.loudness_range == nil)\n"
		"end\n"
		"assert(type(a.leqm) == 'number', a.leqm)\n"
		"assert(#a.channels == film:get('audio_channels'), #a.channels)\n"
		"assert(a.channels[1].name == 'L', a.channels[1].name)\n"
		"assert(near(a.channels[1].peak_db, -6.02), a.channels[1].peak_db)\n"
		"assert(math.type(a.channels[1].peak_time) == 'integer')\n"
		"assert(a.channels[2].name == 'R', a.channels[2].name)\n"
		"assert(near(a.channels[2].peak_db, -12.04), a.channels[2].peak_db)\n"
		/* Nothing is mapped to C */
		"assert(a.channels[3].name == 'C' and a.channels[3].peak_db < -100, a.channels[3].peak_db)\n"
		"assert(film:describe():find('\\naudio peak: -6.02dB\\n', 1, true), film:describe())\n"
		/* As the dialog, a gain change is applied to an analysis of one piece of content without analysing again */
		"c:set('audio.gain', -3)\n"
		"assert(near(c:audio_analysis().peak_db, -9.02), c:audio_analysis().peak_db)\n"
		"assert(near(c:audio_analysis().channels[2].peak_db, -15.04), c:audio_analysis().channels[2].peak_db)\n"
		"f = film:audio_analysis()\n"
		"assert(near(f.peak_db, -9.02), f.peak_db)\n"
		"assert(f.peak_channel == 'L', f.peak_channel)\n"
		);
}


/** c:audio_analysis() refuses content with no audio, as the audio panel shows nothing then */
BOOST_AUTO_TEST_CASE(lua_audio_analysis_refused)
{
	LuaState state;
	film_and_files(state, "lua_audio_analysis_refused", { { "picture", "flat_red.png" } });
	check_runs(state, "film = dcpomatic.open_film(film_dir)\nc = film:add_content(picture)\n");
	check_error(state, "c:audio_analysis()", "c:audio_analysis: the content has no audio (the GUI shows no levels for it)");
	check_error(state, "film.audio_analysis(c)", "film:audio_analysis: argument 1: expected a Film");
	/* The DCP panel's graph is offered for a film with no audio; its levels are silence */
	check_runs(state, "assert(film:audio_analysis().peak_db < -100, film:audio_analysis().peak_db)\n");
}


/** dcpomatic.gain_for_fader_change(from, to [, processor]) is the gain calculator's answer */
BOOST_AUTO_TEST_CASE(lua_gain_for_fader_change)
{
	LuaState state;
	check_runs(state,
		"function near(a, b) return math.abs(a - b) < 1e-4 end\n"
		/* The calculator's first processor is the CP650/CP750: 3.33dB a step above 4, 20dB a step below */
		"assert(near(dcpomatic.gain_for_fader_change(7, 6), -10 / 3), dcpomatic.gain_for_fader_change(7, 6))\n"
		"assert(near(dcpomatic.gain_for_fader_change(3, 4), 20), dcpomatic.gain_for_fader_change(3, 4))\n"
		"assert(near(dcpomatic.gain_for_fader_change(7, 6, 'dolby_cp750'), -10 / 3))\n"
		"assert(near(dcpomatic.gain_for_fader_change(5, 7, 'usl'), 10 * 0.5 + 3.33333333 * 1.5), dcpomatic.gain_for_fader_change(5, 7, 'usl'))\n"
		"assert(near(dcpomatic.gain_for_fader_change(7, 6, 'dataset_ap2x'), -5))\n"
		"assert(near(dcpomatic.gain_for_fader_change(7, 7), 0))\n"
		);
	auto const lua = std::stod(error_from(state, "error(tostring(dcpomatic.gain_for_fader_change(6.5, 3.2, 'usl')), 0)"));
	BOOST_CHECK_CLOSE(lua, CinemaSoundProcessor::from_id("usl")->db_for_fader_change(6.5, 3.2), 1e-4);

	check_error(
		state,
		"dcpomatic.gain_for_fader_change(7, 6, 'cp950')",
		"dcpomatic.gain_for_fader_change: argument 3: unknown sound processor 'cp950'; the processors are "
		"dolby_cp750 (Dolby CP650 or CP750), usl (USL), dataset_ap2x (Datasat AP20 or AP25)"
		);
	check_error(state, "dcpomatic.gain_for_fader_change('7', 6)", "dcpomatic.gain_for_fader_change: argument 1: expected number");
	check_error(state, "dcpomatic.gain_for_fader_change(7)", "dcpomatic.gain_for_fader_change: argument 2: expected number");
}

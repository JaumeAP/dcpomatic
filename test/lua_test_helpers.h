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


#ifndef DCPOMATIC_LUA_TEST_HELPERS_H
#define DCPOMATIC_LUA_TEST_HELPERS_H


/** @file  test/lua_test_helpers.h
 *  @brief Helpers shared by the tests of Lua scripting.
 */


#include "lib/film.h"
#include "lib/job_manager.h"
#include "lua/lua_state.h"
/* test.h has no include guard, so the files that include this header get it from here */
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


inline void
set_global(LuaState& state, std::string const& name, std::string const& value)
{
	lua_pushstring(state.raw(), value.c_str());
	lua_setglobal(state.raw(), name.c_str());
}


inline std::string
get_global(LuaState& state, std::string const& name)
{
	lua_getglobal(state.raw(), name.c_str());
	std::string const value = lua_tostring(state.raw(), -1) ? lua_tostring(state.raw(), -1) : "";
	lua_pop(state.raw(), 1);
	return value;
}


/** Run code, failing the test with Lua's message if it raises an error */
inline void
check_runs(LuaState& state, std::string const& code)
{
	try {
		state.run_string(code);
	} catch (LuaError& e) {
		BOOST_ERROR(e.what());
	}
}


/** @return the message of the error that code raises, or an empty string if it raises none */
inline std::string
error_from(LuaState& state, std::string const& code)
{
	try {
		state.run_string(code);
	} catch (LuaError& e) {
		return e.what();
	}
	return {};
}


/** Write a 48kHz, 16-bit PCM WAV of the given length in which every sample is 1000 */
inline void
write_wav(boost::filesystem::path path, int channels, int seconds)
{
	int const rate = 48000;
	int const data_size = rate * seconds * channels * 2;

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
	for (int i = 0; i < rate * seconds * channels; ++i) {
		u16(1000);
	}
}


/** @return a film called name, saved, with nothing in it, and the path of a new WAV for it */
inline boost::filesystem::path
film_and_wav(LuaState& state, std::string const& name, int channels, int seconds)
{
	auto film = new_test_film(name);
	auto const wav = test_film_dir(name + "_content") / "sound.wav";
	boost::filesystem::remove_all(wav.parent_path());
	boost::filesystem::create_directories(wav.parent_path());
	write_wav(wav, channels, seconds);
	set_global(state, "film_dir", film->directory()->string());
	set_global(state, "wav", wav.string());
	return *film->directory();
}


/** Make a saved, empty film, a mono WAV (1 second unless told) and the path of a still picture, as the globals film_dir, wav and picture */
inline void
picture_and_sound(LuaState& state, std::string const& name, int seconds = 1)
{
	film_and_wav(state, name, 1, seconds);
	set_global(state, "picture", boost::filesystem::canonical("test/data/flat_red.png").string());
}


/** Make a saved, empty film and set the globals film_dir and, for each of files, name = the canonical path of test/data/<file> */
inline void
film_and_files(LuaState& state, std::string const& name, std::vector<std::pair<std::string, std::string>> const& files)
{
	auto film = new_test_film(name);
	set_global(state, "film_dir", film->directory()->string());
	for (auto const& file: files) {
		set_global(state, file.first, boost::filesystem::canonical(boost::filesystem::path("test/data") / file.second).string());
	}
}


/** Say why (part of) a test is skipped; BOOST_TEST_MESSAGE would show it only with --log_level=message */
inline void
skipping(std::string const& message)
{
	std::cerr << message << "\n";
}


/** Drops JobManager when it goes, even when a test fails, so that a failed job a test leaves behind
 *  (such as the CheckContentJob of a film with a missing file) does not fail the next test's wait_for_jobs().
 */
class DropJobsAtEnd
{
public:
	DropJobsAtEnd() = default;
	DropJobsAtEnd(DropJobsAtEnd const&) = delete;
	DropJobsAtEnd& operator=(DropJobsAtEnd const&) = delete;

	~DropJobsAtEnd()
	{
		JobManager::drop();
	}
};


/** Collects what is written to std::cerr while it exists */
class CerrCapture
{
public:
	CerrCapture()
		: _old(std::cerr.rdbuf(_buffer.rdbuf()))
	{}

	~CerrCapture()
	{
		std::cerr.rdbuf(_old);
	}

	/** @return true if a line that starts with `warning: ' contains part */
	bool has_warning(std::string const& part) const
	{
		std::istringstream lines(_buffer.str());
		std::string line;
		while (std::getline(lines, line)) {
			if (line.compare(0, 9, "warning: ") == 0 && line.find(part) != std::string::npos) {
				return true;
			}
		}
		return false;
	}

	std::string text() const
	{
		return _buffer.str();
	}

private:
	std::ostringstream _buffer;
	std::streambuf* _old;
};


#endif

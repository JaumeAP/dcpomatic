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


/** @file  src/lua/units.cc
 *  @brief The dcpomatic.seconds, frames and timecode helpers, which return ticks.
 *
 *  These are called from Lua, which unwinds with longjmp when luaL_error is called, so
 *  they keep no C++ objects with destructors alive across a call that can raise.
 */


#include "lua_state.h"
#include "lib/dcpomatic_time.h"
#include <cmath>
#include <cstdio>


using namespace dcpomatic;


static bool
is_whole(double ticks)
{
	return std::isfinite(ticks) && std::fabs(ticks - std::llround(ticks)) <= 1e-6;
}


/** dcpomatic.seconds(x): x seconds as ticks.  Raises an error if that is not a whole number of ticks. */
static int
seconds(lua_State* state)
{
	auto const s = luaL_checknumber(state, 1);
	if (!is_whole(s * ContentTime::HZ)) {
		return luaL_error(state, "%f seconds is not a whole number of ticks (%d to the second)", s, ContentTime::HZ);
	}

	lua_pushinteger(state, ContentTime::from_seconds(s).get());
	return 1;
}


/** dcpomatic.frames(n, fps): n frames at fps as ticks */
static int
frames(lua_State* state)
{
	auto const n = luaL_checkinteger(state, 1);
	auto const fps = luaL_checknumber(state, 2);
	if (!(fps > 0)) {
		return luaL_error(state, "frame rate must be positive");
	}

	/* from_frames() would truncate a fractional tick, so check first */
	if (!is_whole(static_cast<double>(n) * ContentTime::HZ / fps)) {
		return luaL_error(state, "%d frames at %f fps is not a whole number of ticks", static_cast<int>(n), fps);
	}

	lua_pushinteger(state, std::llround(static_cast<double>(n) * ContentTime::HZ / fps));
	return 1;
}


/** dcpomatic.timecode("HH:MM:SS:FF", fps): a timecode at fps as ticks */
static int
timecode(lua_State* state)
{
	auto const text = luaL_checkstring(state, 1);
	auto const fps = luaL_checknumber(state, 2);
	if (!(fps > 0)) {
		return luaL_error(state, "frame rate must be positive");
	}

	int h, m, s, f;
	char extra;
	if (std::sscanf(text, "%d:%d:%d:%d%c", &h, &m, &s, &f, &extra) != 4 || h < 0 || m < 0 || s < 0 || f < 0 || f >= fps) {
		return luaL_error(state, "bad timecode '%s' for %f fps: expected HH:MM:SS:FF", text, fps);
	}

	/* ContentTime(HMSF, fps) would truncate a fractional tick in the frame part, so check first */
	auto const frame_ticks = static_cast<double>(f) * ContentTime::HZ / fps;
	if (!is_whole(frame_ticks)) {
		return luaL_error(state, "timecode '%s' at %f fps is not a whole number of ticks", text, fps);
	}

	auto const whole_seconds = ContentTime::from_seconds(h * 3600.0 + m * 60.0 + s);
	lua_pushinteger(state, (whole_seconds + ContentTime(std::llround(frame_ticks))).get());
	return 1;
}


void
register_units(lua_State* state)
{
	static luaL_Reg const functions[] = {
		{ "seconds", seconds },
		{ "frames", frames },
		{ "timecode", timecode },
		{ nullptr, nullptr }
	};

	lua_getglobal(state, "dcpomatic");
	if (lua_isnil(state, -1)) {
		lua_pop(state, 1);
		lua_newtable(state);
	}
	luaL_setfuncs(state, functions, 0);
	lua_setglobal(state, "dcpomatic");
}

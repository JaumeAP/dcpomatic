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


/** @file  src/lua/mapping_binding.cc
 *  @brief The audio mapping methods of dcpomatic.Content.
 *
 *  Channels are numbered from 1 in Lua, from 0 in AudioMapping.  Like the GUI, every change reads
 *  AudioContent::mapping(), changes it and gives it back with AudioContent::set_mapping().
 */


#include "content_binding.h"
#include "lib/audio_content.h"
#include "lib/audio_mapping.h"
#include "lib/content.h"


using std::shared_ptr;


static shared_ptr<AudioContent>
check_audio(lua_State* state)
{
	auto audio = check_content(state, 1).content->audio;
	if (!audio) {
		throw LuaError("the content has no audio");
	}
	return audio;
}


/** @return the channel number at index, from 0, checking that it is between 1 and count */
static int
check_channel(lua_State* state, int index, int count, char const* what)
{
	int channel = 0;
	try {
		channel = to_int(state, index);
	} catch (PropertyValueError& e) {
		throw LuaError(fmt::format("argument {}: {}", index, e.what()));
	}
	if (channel < 1 || channel > count) {
		throw LuaError(fmt::format("{} channel {} is not between 1 and {}", what, channel, count));
	}
	return channel - 1;
}


/** @return table[input][output] = gain, for every input of the content and every DCP channel */
int
content_mapping(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto const mapping = check_audio(state)->mapping();
		lua_newtable(state);
		for (int input = 0; input < mapping.input_channels(); ++input) {
			lua_newtable(state);
			for (int output = 0; output < mapping.output_channels(); ++output) {
				lua_pushnumber(state, mapping.get(input, output));
				lua_rawseti(state, -2, output + 1);
			}
			lua_rawseti(state, -2, input + 1);
		}
		return 1;
	});
}


/** c:map(input, output, gain): set the gain (linear, 1 for unity) from an input to a DCP channel */
int
content_map(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto audio = check_audio(state);
		auto mapping = audio->mapping();
		auto const input = check_channel(state, 2, mapping.input_channels(), "input");
		auto const output = check_channel(state, 3, mapping.output_channels(), "output");
		double gain = 0;
		try {
			gain = to_double(state, 4);
		} catch (PropertyValueError& e) {
			throw LuaError(fmt::format("argument 4: {}", e.what()));
		}
		mapping.set(input, output, static_cast<float>(gain));
		audio->set_mapping(mapping);
		return 0;
	});
}


/** c:clear_mapping(): map nothing to anything */
int
content_clear_mapping(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto audio = check_audio(state);
		auto mapping = audio->mapping();
		mapping.make_zero();
		audio->set_mapping(mapping);
		return 0;
	});
}

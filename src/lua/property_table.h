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


/** @file  src/lua/property_table.h
 *  @brief PropertyTable, a list of named settings of a library object that Lua can get and set,
 *  and the converters its rows use.
 *
 *  Lua is built as C, so luaL_error unwinds with longjmp and would skip C++ destructors.
 *  Code here therefore reports errors by throwing; lua_protect() turns the exception into a Lua
 *  error once no C++ object is left alive.
 */


#ifndef DCPOMATIC_LUA_PROPERTY_TABLE_H
#define DCPOMATIC_LUA_PROPERTY_TABLE_H


#include "lua_state.h"
#include "lib/dcpomatic_assert.h"
#include "lib/dcpomatic_time.h"
#include <fmt/format.h>
#include <boost/optional.hpp>
#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>


/** Thrown when a value given to a property cannot be used; PropertyTable::set adds the property's name */
class PropertyValueError : public std::runtime_error
{
public:
	explicit PropertyValueError(std::string const& message)
		: std::runtime_error(message)
	{}
};


/** A setter that has no row in a PropertyTable, and why */
struct SkippedSetter
{
	std::string name;
	std::string reason;
};


inline bool
to_bool(lua_State* state, int index)
{
	if (lua_type(state, index) != LUA_TBOOLEAN) {
		throw PropertyValueError("expected boolean");
	}
	return lua_toboolean(state, index);
}


/** @return the integer at index; a float is accepted only if it is a whole number */
inline int64_t
to_int64(lua_State* state, int index)
{
	int is_integer = 0;
	auto const value = lua_type(state, index) == LUA_TNUMBER ? lua_tointegerx(state, index, &is_integer) : 0;
	if (!is_integer) {
		throw PropertyValueError("expected integer");
	}
	return value;
}


inline int
to_int(lua_State* state, int index)
{
	auto const value = to_int64(state, index);
	if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max()) {
		throw PropertyValueError("expected integer");
	}
	return static_cast<int>(value);
}


inline double
to_double(lua_State* state, int index)
{
	if (lua_type(state, index) != LUA_TNUMBER) {
		throw PropertyValueError("expected number");
	}
	return lua_tonumber(state, index);
}


/** @return the string at index; unlike lua_tostring this does not accept a number */
inline std::string
to_string(lua_State* state, int index)
{
	if (lua_type(state, index) != LUA_TSTRING) {
		throw PropertyValueError("expected string");
	}
	size_t length = 0;
	auto const data = lua_tolstring(state, index, &length);
	return std::string(data, length);
}


inline boost::optional<std::string>
to_optional_string(lua_State* state, int index)
{
	if (lua_isnil(state, index)) {
		return {};
	}
	return to_string(state, index);
}


inline void
push_string(lua_State* state, std::string const& value)
{
	lua_pushlstring(state, value.c_str(), value.size());
}


inline void
push_optional_string(lua_State* state, boost::optional<std::string> value)
{
	if (value) {
		push_string(state, *value);
	} else {
		lua_pushnil(state);
	}
}


/** Push value[key] of the table at index (without metamethods) */
inline void
raw_field(lua_State* state, int index, char const* key)
{
	index = lua_absindex(state, index);
	lua_pushstring(state, key);
	lua_rawget(state, index);
}


/** Call parse(text), turning any exception it throws (often a failed DCPOMATIC_ASSERT) into PropertyValueError */
template <class F>
auto
parse(std::string const& text, F parse) -> decltype(parse(text))
{
	try {
		return parse(text);
	} catch (std::exception&) {
		throw PropertyValueError(fmt::format("unknown value '{}'", text));
	}
}


inline dcpomatic::ContentTime
to_content_time(lua_State* state, int index)
{
	try {
		return dcpomatic::ContentTime(to_int64(state, index));
	} catch (PropertyValueError&) {
		throw PropertyValueError("expected integer ticks");
	}
}


inline dcpomatic::DCPTime
to_dcp_time(lua_State* state, int index)
{
	try {
		return dcpomatic::DCPTime(to_int64(state, index));
	} catch (PropertyValueError&) {
		throw PropertyValueError("expected integer ticks");
	}
}


/** @return the number of single-character edits that turn a into b */
inline size_t
edit_distance(std::string const& a, std::string const& b)
{
	std::vector<size_t> row(b.size() + 1);
	for (size_t j = 0; j <= b.size(); ++j) {
		row[j] = j;
	}

	for (size_t i = 1; i <= a.size(); ++i) {
		auto diagonal = row[0];
		row[0] = i;
		for (size_t j = 1; j <= b.size(); ++j) {
			auto const above = row[j];
			row[j] = std::min({ row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1) });
			diagonal = above;
		}
	}

	return row[b.size()];
}


/** The settings of a T that Lua can see, one row per setter, in the order they were added */
template <class T>
class PropertyTable
{
public:
	/** Set the property of a T from the Lua value at index; throw PropertyValueError if the value cannot be used */
	using Setter = std::function<void (lua_State*, T&, int index)>;
	/** Push the value of the property of a T onto the Lua stack */
	using Getter = std::function<void (lua_State*, T const&)>;

	void add(std::string name, Setter set, Getter get)
	{
		_rows.push_back({ name, set, get });
	}

	/** @throw LuaError, naming the property, if there is no such property or the value at index cannot be used */
	void set(lua_State* state, T& object, std::string const& name, int index) const
	{
		auto const& r = row(name);
		try {
			r.set(state, object, index);
		} catch (LuaError&) {
			/* A row's own LuaError already names the property */
			throw;
		} catch (std::exception& e) {
			/* PropertyValueError, or the library refusing the value (often a failed DCPOMATIC_ASSERT) */
			throw LuaError(fmt::format("property '{}': {}", name, e.what()));
		}
	}

	/** Push the value of a property onto the Lua stack.
	 *  @throw LuaError if there is no such property.
	 */
	void get(lua_State* state, T const& object, std::string const& name) const
	{
		row(name).get(state, object);
	}

	bool has(std::string const& name) const
	{
		return std::find_if(_rows.begin(), _rows.end(), [&name](Row const& r) { return r.name == name; }) != _rows.end();
	}

	std::vector<std::string> names() const
	{
		std::vector<std::string> n;
		for (auto const& r: _rows) {
			n.push_back(r.name);
		}
		return n;
	}

private:
	struct Row
	{
		std::string name;
		Setter set;
		Getter get;
	};

	Row const& row(std::string const& name) const
	{
		auto i = std::find_if(_rows.begin(), _rows.end(), [&name](Row const& r) { return r.name == name; });
		if (i != _rows.end()) {
			return *i;
		}

		auto closest = std::min_element(_rows.begin(), _rows.end(), [&name](Row const& a, Row const& b) {
			return edit_distance(name, a.name) < edit_distance(name, b.name);
		});
		DCPOMATIC_ASSERT(closest != _rows.end());
		throw LuaError(fmt::format("unknown property '{}' (did you mean '{}'?)", name, closest->name));
	}

	std::vector<Row> _rows;
};


/** Call f(state) for a lua_CFunction, turning any exception it throws into a Lua error.
 *  @return what f returns: the number of values it pushed.
 */
template <class F>
int
lua_protect(lua_State* state, F f)
{
	{
		try {
			return f(state);
		} catch (std::exception& e) {
			luaL_where(state, 1);
			lua_pushstring(state, e.what());
			lua_concat(state, 2);
		} catch (...) {
			/* Nothing may be thrown through Lua's C frames */
			luaL_where(state, 1);
			lua_pushstring(state, "unknown error");
			lua_concat(state, 2);
		}
	}

	/* The exception has gone now, so it is safe for lua_error to longjmp past this frame */
	return lua_error(state);
}


/** @return the string argument at index; @throw LuaError if it is not a string */
inline std::string
check_string(lua_State* state, int index)
{
	try {
		return to_string(state, index);
	} catch (PropertyValueError& e) {
		throw LuaError(fmt::format("argument {}: {}", index, e.what()));
	}
}


/** Make the metatable called name, with methods as its __index and gc as its __gc */
inline void
register_metatable(lua_State* state, char const* name, luaL_Reg const* methods, lua_CFunction gc)
{
	luaL_newmetatable(state, name);
	lua_newtable(state);
	luaL_setfuncs(state, methods, 0);
	lua_setfield(state, -2, "__index");
	lua_pushcfunction(state, gc);
	lua_setfield(state, -2, "__gc");
	lua_pop(state, 1);
}


class AudioContent;
class Content;
class FFmpegContent;
class Film;
class TextContent;
class VideoContent;

/** One row per Film::set_* that a script can use (name without `set_') */
PropertyTable<Film> const& film_properties();
/** The Film::set_* that have no row in film_properties() */
std::vector<SkippedSetter> const& film_skipped_setters();


/** A piece of content and its film, which Content::set_position needs */
struct ContentContext
{
	std::shared_ptr<Film> film;
	std::shared_ptr<Content> content;
};

/** One row per Content::set_* that a script can use (name without `set_') */
PropertyTable<ContentContext> const& content_properties();
std::vector<SkippedSetter> const& content_skipped_setters();
/** @return true if content is still image content, the only content whose video.length can be used */
bool is_still(std::shared_ptr<const Content> content);
/** One row per VideoContent::set_*, named `video.<name without set_>' */
PropertyTable<VideoContent> const& video_properties();
std::vector<SkippedSetter> const& video_skipped_setters();
/** One row per AudioContent::set_*, named `audio.<name without set_>' */
PropertyTable<AudioContent> const& audio_properties();
std::vector<SkippedSetter> const& audio_skipped_setters();
/** One row per TextContent::set_*, named `text.<name without set_>' */
PropertyTable<TextContent> const& text_properties();
std::vector<SkippedSetter> const& text_skipped_setters();
/** One row per FFmpegContent::set_*, named `ffmpeg.<name without set_>' */
PropertyTable<FFmpegContent> const& ffmpeg_properties();
std::vector<SkippedSetter> const& ffmpeg_skipped_setters();


class DCPContent;

/** DCP content, its film, and the original type of the text that dcp.reference_text is for, if one was given */
struct DCPContext
{
	std::shared_ptr<Film> film;
	std::shared_ptr<DCPContent> dcp;
	boost::optional<std::string> text_type;
};

/** One row per DCPContent::set_*, named `dcp.<name without set_>' */
PropertyTable<DCPContext> const& dcp_properties();
std::vector<SkippedSetter> const& dcp_skipped_setters();


#endif

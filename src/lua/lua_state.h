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


/** @file  src/lua/lua_state.h
 *  @brief A Lua interpreter with the dcpomatic table registered.
 */


#ifndef DCPOMATIC_LUA_STATE_H
#define DCPOMATIC_LUA_STATE_H


#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <lua.hpp>
#include <list>
#include <stdexcept>
#include <string>
#include <vector>


/** An error raised by a Lua script, or while loading one */
class LuaError : public std::runtime_error
{
public:
	explicit LuaError(std::string const& message)
		: std::runtime_error(message)
	{}
};


/** Owns a lua_State and runs scripts in it.  Times in scripts are integers in ticks,
 *  96000 to the second.
 */
class LuaState
{
public:
	LuaState();
	~LuaState();

	LuaState(LuaState const&) = delete;
	LuaState& operator=(LuaState const&) = delete;

	/** @param code Lua source.
	 *  @param chunk_name Name used in error messages, as `chunk_name:line:'.
	 *  @throw LuaError with Lua's message if the code does not load or raises an error.
	 */
	void run_string(std::string const& code, std::string const& chunk_name = "script");

	/** @throw LuaError with Lua's message if the file cannot be read or its code raises an error */
	void run_file(boost::filesystem::path path);

	lua_State* raw() {
		return _state;
	}

	/** @return the LuaState that made state */
	static LuaState& owner(lua_State* state);

	/** Print `warning: <text>' to stderr and keep text, on one line, for warnings().
	 *  Safe to call from any thread, and never throws, so signal handlers can use it.
	 */
	void warning(std::string const& text);
	std::vector<std::string> warnings() const;
	void clear_warnings();

	/** Keep connection until this state is destroyed */
	void keep(boost::signals2::connection connection);

private:
	void run_loaded();

	lua_State* _state;

	mutable boost::mutex _warnings_mutex;
	std::vector<std::string> _warnings;
	/** Destroyed after the destructor's lua_close, so no handler can see a half-destroyed state */
	std::list<boost::signals2::scoped_connection> _connections;
};


void register_units(lua_State* state);
void register_film(lua_State* state);
void register_content(lua_State* state);
void register_warnings(lua_State* state);


#endif

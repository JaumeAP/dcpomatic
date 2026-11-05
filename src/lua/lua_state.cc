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


#include "lua_state.h"
#include "lib/dcpomatic_assert.h"
#include <boost/regex.hpp>
#include <clocale>
#include <iostream>


using std::string;
using std::vector;


/** Its address is the registry key of the LuaState that owns a lua_State */
static char const owner_key = 0;


LuaState::LuaState()
	: _state(luaL_newstate())
{
	if (!_state) {
		throw LuaError("could not create a Lua state");
	}

	/* Script output is data, so numbers must not take the decimal comma of the environment's locale,
	 * which Glib::init() (from Pango::init() in dcpomatic_setup()) sets.
	 */
	setlocale(LC_NUMERIC, "C");

	try {
		lua_pushlightuserdata(_state, this);
		lua_rawsetp(_state, LUA_REGISTRYINDEX, &owner_key);

		luaL_openlibs(_state);
		register_units(_state);
		register_film(_state);
		register_content(_state);
		register_warnings(_state);
	} catch (...) {
		/* The destructor does not run when the constructor throws */
		lua_close(_state);
		throw;
	}
}


LuaState&
LuaState::owner(lua_State* state)
{
	lua_rawgetp(state, LUA_REGISTRYINDEX, &owner_key);
	auto owner = static_cast<LuaState*>(lua_touserdata(state, -1));
	lua_pop(state, 1);
	DCPOMATIC_ASSERT(owner);
	return *owner;
}


void
LuaState::warning(string const& text)
{
	try {
		/* One line each, so that a reader of stderr can tell where a warning ends */
		auto const line = boost::regex_replace(text, boost::regex("\\s*\\n\\s*"), " ");
		boost::mutex::scoped_lock lm(_warnings_mutex);
		std::cerr << "warning: " << line << "\n";
		_warnings.push_back(line);
	} catch (...) {
		/* Called from signal handlers, which must not throw into the library or through Lua */
	}
}


vector<string>
LuaState::warnings() const
{
	boost::mutex::scoped_lock lm(_warnings_mutex);
	return _warnings;
}


void
LuaState::clear_warnings()
{
	boost::mutex::scoped_lock lm(_warnings_mutex);
	_warnings.clear();
}


void
LuaState::keep(boost::signals2::connection connection)
{
	_connections.emplace_back(connection);
}


LuaState::~LuaState()
{
	lua_close(_state);
}


/** Call the chunk on top of the stack, throwing LuaError if loading it (already done by the caller) or running it failed */
void
LuaState::run_loaded()
{
	if (lua_pcall(_state, 0, 0, 0) != LUA_OK) {
		string const message = lua_tostring(_state, -1) ? lua_tostring(_state, -1) : "unknown Lua error";
		lua_pop(_state, 1);
		throw LuaError(message);
	}
}


void
LuaState::run_string(string const& code, string const& chunk_name)
{
	/* A leading `=' makes Lua use the name as it is, giving `script:1:' rather than `[string "script"]:1:' */
	auto const name = "=" + chunk_name;
	if (luaL_loadbuffer(_state, code.c_str(), code.size(), name.c_str()) != LUA_OK) {
		string const message = lua_tostring(_state, -1) ? lua_tostring(_state, -1) : "unknown Lua error";
		lua_pop(_state, 1);
		throw LuaError(message);
	}

	run_loaded();
}


void
LuaState::run_file(boost::filesystem::path path)
{
	if (luaL_loadfile(_state, path.string().c_str()) != LUA_OK) {
		string const message = lua_tostring(_state, -1) ? lua_tostring(_state, -1) : "unknown Lua error";
		lua_pop(_state, 1);
		throw LuaError(message);
	}

	run_loaded();
}

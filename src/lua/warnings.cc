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


/** @file  src/lua/warnings.cc
 *  @brief dcpomatic.warnings() and dcpomatic.clear_warnings(), and the library signals whose
 *  messages the GUI shows in dialogs, connected to the warnings of a LuaState.
 *
 *  The signal handlers only call LuaState::warning: they hold no Lua stack values and never throw.
 */


#include "lua_state.h"
#include "property_table.h"
#include "warnings.h"
#include "lib/analytics.h"
#include "lib/config.h"
#include "lib/film.h"
#include "lib/variant.h"
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>


using std::shared_ptr;
using std::string;


void
lua_warning(lua_State* state, string const& text)
{
	LuaState::owner(state).warning(text);
}


void
watch_film(lua_State* state, shared_ptr<Film> film)
{
	auto& owner = LuaState::owner(state);
	owner.keep(film->Message.connect([&owner](string const& message) { owner.warning(message); }));
}


/** What the GUI's dialog says about reason; the script never re-creates the chain, as the dialog's default does not */
static string
bad_config_message(Config::BadReason reason)
{
	switch (reason) {
	case Config::BAD_SIGNER_UTF8_STRINGS:
		return variant::insert_dcpomatic("The certificate chain that {} uses for signing DCPs and KDMs contains a small error which will prevent DCPs from being validated correctly on some systems.  It has not been re-created.");
	case Config::BAD_SIGNER_INCONSISTENT:
		return variant::insert_dcpomatic("The certificate chain that {} uses for signing DCPs and KDMs is inconsistent and cannot be used.  It has not been re-created.");
	case Config::BAD_DECRYPTION_INCONSISTENT:
		return variant::insert_dcpomatic("The certificate chain that {} uses for decrypting KDMs is inconsistent and cannot be used.  It has not been re-created.");
	case Config::BAD_SIGNER_VALIDITY_TOO_LONG:
		return variant::insert_dcpomatic("The certificate chain that {} uses for signing DCPs and KDMs has a validity period that is too long.  This will cause problems playing back DCPs on some systems.  It has not been re-created.");
	case Config::BAD_SIGNER_DN_QUALIFIER:
		return variant::insert_dcpomatic("The certificate chain that {} uses for signing DCPs and KDMs contains a small error, caused by a bug that has now been fixed, which will prevent DCPs from being validated correctly on some systems.  It has not been re-created.");
	}

	return "The configuration has a problem.";
}


static int
warnings(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		lua_newtable(state);
		int n = 1;
		for (auto const& warning: LuaState::owner(state).warnings()) {
			push_string(state, warning);
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


static int
clear_warnings(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		LuaState::owner(state).clear_warnings();
		return 0;
	});
}


void
register_warnings(lua_State* state)
{
	static luaL_Reg const functions[] = {
		{ "warnings", warnings },
		{ "clear_warnings", clear_warnings },
		{ nullptr, nullptr }
	};

	lua_getglobal(state, "dcpomatic");
	if (lua_isnil(state, -1)) {
		lua_pop(state, 1);
		lua_newtable(state);
	}
	luaL_setfuncs(state, functions, 0);
	lua_setglobal(state, "dcpomatic");

	auto& owner = LuaState::owner(state);

	/* Connected before Config::instance() below, so that problems found while reading the configuration are seen */
	/* Every handler catches everything: an exception from one called inside Config::read would make it back up
	 * the configuration and replace it with defaults, including new certificate chains.
	 */
	owner.keep(Config::FailedToLoad.connect([&owner](Config::LoadFailure) {
		owner.warning("The existing configuration failed to load.  Default values will be used instead.");
	}));
	owner.keep(Config::Warning.connect([&owner](string const& message) { owner.warning(message); }));
	owner.keep(Config::Bad.connect([&owner](Config::BadReason reason) {
		try {
			owner.warning(bad_config_message(reason));
		} catch (...) {}
		/* As if the GUI's dialog had been answered `Do nothing' */
		return false;
	}));
	owner.keep(Config::instance()->DKDMsChangedByOtherProgram.connect([&owner]() {
		try {
			owner.warning(variant::insert_dcpomatic("Another {} program has added or removed DKDMs since this one last looked at them.  The list of DKDMs will be updated to include those changes before it is saved."));
		} catch (...) {}
	}));
	owner.keep(Analytics::instance()->Message.connect([&owner](string const& title, string const& html) {
		try {
			/* The GUI shows this as HTML; a script wants the words */
			owner.warning(title + " " + strip_markup(html));
		} catch (...) {}
	}));
}


string
strip_markup(string const& text)
{
	auto stripped = boost::regex_replace(text, boost::regex("(\\s*<[^>]*>\\s*)+"), " ");
	boost::algorithm::trim(stripped);
	return stripped;
}

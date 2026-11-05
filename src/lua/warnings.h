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


/** @file  src/lua/warnings.h
 *  @brief The warnings that the GUI would show in a dialog, given to a script instead.
 */


#ifndef DCPOMATIC_LUA_WARNINGS_H
#define DCPOMATIC_LUA_WARNINGS_H


#include <lua.hpp>
#include <memory>
#include <string>


class Film;


/** Print `warning: <text>' to stderr and add text to dcpomatic.warnings() of state */
void lua_warning(lua_State* state, std::string const& text);

/** Make the messages that film sends through Film::Message warnings of state */
void watch_film(lua_State* state, std::shared_ptr<Film> film);

/** @return text with its HTML or Pango markup tags replaced by single spaces, trimmed */
std::string strip_markup(std::string const& text);


#endif

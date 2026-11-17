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


/** @file  src/lua/content_binding.h
 *  @brief The dcpomatic.Content userdata, shared by the Film binding and the mapping methods.
 */


#ifndef DCPOMATIC_LUA_CONTENT_BINDING_H
#define DCPOMATIC_LUA_CONTENT_BINDING_H


#include "property_table.h"
#include <list>


class Job;
class RGBA;


void push_content(lua_State* state, std::shared_ptr<Content> content, std::shared_ptr<Film> film);

/** @return the content and film of the dcpomatic.Content at index.
 *  @throw LuaError if it is not a Content, or its content is no longer in its film.
 */
ContentContext check_content(lua_State* state, int index);

/** @return true if the value at index is a dcpomatic.Content */
bool is_content(lua_State* state, int index);

/** c:mapping(), c:map(input, output, gain) and c:clear_mapping() */
int content_mapping(lua_State* state);
int content_map(lua_State* state);
int content_clear_mapping(lua_State* state);

/** c:reexamine(), in film_binding.cc as it waits for its job as the film's calls do */
int content_reexamine(lua_State* state);

/** c:audio_analysis(), in film_binding.cc as it waits for its job as the film's calls do */
int content_audio_analysis(lua_State* state);

/** Examine content again and wait for it, as the GUI's Re-examine, Choose CPL, Add OV and Add KDM do */
void examine_content(lua_State* state, std::shared_ptr<Content> content);

/** Push a list of { id=, title= } for the CPLs of a DCP, as the content menu's Choose CPL lists them.
 *  @throw the library's exception, without pushing anything, if the DCP cannot be read.
 */
void push_cpls(lua_State* state, std::shared_ptr<const DCPContent> dcp);

/** Push a list of { category=, sub_heading=, name=, value=, unit= }, one per Content::user_properties entry */
void push_user_properties(lua_State* state, ContentContext const& context);

/** @return what the content list marks the content with: `missing', `needs KDM', `needs OV' */
std::vector<std::string> content_status(std::shared_ptr<const Content> content);

/** @return colour as the RRGGBBAA hex string that c:subtitle_colours() gives */
std::string rgba_to_string(RGBA colour);

/** Wait until JobManager has nothing to do, as film_binding.cc does for every job it starts; jobs' messages become warnings.
 *  @param before Jobs that existed before; their errors and messages are not ours to report.
 *  @param errors_are_warnings true to make the errors of new jobs warnings instead.
 *  @throw LuaError with the message of the first new job that failed, unless errors_are_warnings.
 */
void wait_for_jobs(lua_State* state, std::list<std::shared_ptr<Job>> const& before, bool errors_are_warnings = false);

/** @return the boolean field key of the table of options at index; false if it is not there, or there is no table */
bool check_option(lua_State* state, int index, char const* key);

/** @throw LuaError if the table of options at index (if there is one) has a key that is not in known */
void check_known_options(lua_State* state, int index, std::vector<std::string> const& known);


#endif

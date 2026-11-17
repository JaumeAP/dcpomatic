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


/** @file  src/lua/film_binding.cc
 *  @brief The Film userdata, and dcpomatic.open_film / new_film.
 *
 *  Nothing here writes the film's metadata except film:save(), film:make_dcp() and dcpomatic.new_film.
 */


#include "content_binding.h"
#include "property_table.h"
#include "warnings.h"
#include "lib/audio_analysis.h"
#include "lib/audio_content.h"
#include "lib/check_content_job.h"
#include "lib/cinema_sound_processor.h"
#include "lib/config.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/copy_dcp_details_to_film.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/examine_content_job.h"
#include "lib/exceptions.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_film_encoder.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/film.h"
#include "lib/film_util.h"
#include "lib/font.h"
#include "lib/find_missing.h"
#include "lib/hints.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/make_dcp.h"
#include "lib/maths_util.h"
#include "lib/playlist.h"
#include "lib/remembered_asset.h"
#include "lib/signal_manager.h"
#include "lib/subtitle_film_encoder.h"
#include "lib/text_content.h"
#include "lib/transcode_job.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include <dcp/filesystem.h>
#include <dcp/types.h>
#include <libintl.h>
#include <libxml++/libxml++.h>
#include <algorithm>
#include <list>
#include <new>


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


static char const* const film_metatable = "dcpomatic.Film";


static void
push_film(lua_State* state, shared_ptr<Film> film)
{
	new (lua_newuserdatauv(state, sizeof(shared_ptr<Film>), 0)) shared_ptr<Film>(film);
	luaL_setmetatable(state, film_metatable);
}


static shared_ptr<Film>
check_film(lua_State* state, int index)
{
	auto film = static_cast<shared_ptr<Film>*>(luaL_testudata(state, index, film_metatable));
	if (!film) {
		throw LuaError(fmt::format("argument {}: expected a Film", index));
	}
	return *film;
}


/** Wait until JobManager has nothing to do, running the signals that jobs emit for this thread.
 *  The messages that new jobs leave for the user become warnings, as the GUI shows them in a dialog.
 *  @param before Jobs that existed before; their errors and messages are not ours to report.
 *  @param errors_are_warnings true to make the errors of new jobs warnings, as the GUI only lists them in its jobs panel.
 *  @throw LuaError with the message of the first new job that failed, unless errors_are_warnings.
 */
void
wait_for_jobs(lua_State* state, list<shared_ptr<Job>> const& before, bool errors_are_warnings)
{
	DCPOMATIC_ASSERT(signal_manager);
	auto jm = JobManager::instance();

	while (true) {
		/* A job emits Finished just after it stops counting as work to do, so poll the
		 * signals once more after the sleep that follows the last busy check.
		 */
		bool const busy = jm->work_to_do();
		dcpomatic_sleep_milliseconds(100);
		while (signal_manager->ui_idle()) {}
		if (!busy && !jm->work_to_do()) {
			break;
		}
	}

	for (auto job: jm->get()) {
		auto const message = job->message();
		if (message && std::find(before.begin(), before.end(), job) == before.end()) {
			lua_warning(state, fmt::format("{}: {}", job->name(), *message));
		}
	}

	for (auto job: jm->get()) {
		if (job->finished_in_error() && std::find(before.begin(), before.end(), job) == before.end()) {
			auto message = fmt::format("{}: {}", job->name(), job->error_summary());
			if (!job->error_details().empty()) {
				message += fmt::format(" ({})", job->error_details());
			}
			if (!errors_are_warnings) {
				throw LuaError(message);
			}
			lua_warning(state, message);
		}
	}
}


/** @return the hints that the GUI shows before making a DCP of film, in the order they were found; each is also a warning */
static vector<string>
find_hints(lua_State* state, shared_ptr<const Film> film)
{
	DCPOMATIC_ASSERT(signal_manager);
	vector<string> hints;
	bool finished = false;

	Hints hint_finder(film);
	hint_finder.Hint.connect([&hints](string hint) {
		hints.push_back(hint);
	});
	hint_finder.Finished.connect([&finished]() {
		finished = true;
	});

	hint_finder.start();
	while (!finished) {
		signal_manager->ui_idle();
		dcpomatic_sleep_milliseconds(200);
	}
	hint_finder.rethrow();

	for (auto const& hint: hints) {
		lua_warning(state, hint);
	}
	return hints;
}


/** Append a Lua value to text: nil as nothing, a list as `a, b', a table of fields as `{k=v, ...}' */
static void
describe_value(lua_State* state, int index, string& text)
{
	index = lua_absindex(state, index);
	switch (lua_type(state, index)) {
	case LUA_TNIL:
		break;
	case LUA_TBOOLEAN:
		text += lua_toboolean(state, index) ? "true" : "false";
		break;
	case LUA_TNUMBER:
		text += lua_isinteger(state, index) ? fmt::to_string(lua_tointeger(state, index)) : fmt::to_string(lua_tonumber(state, index));
		break;
	case LUA_TSTRING:
		text += lua_tostring(state, index);
		break;
	case LUA_TTABLE:
	{
		auto const size = static_cast<lua_Integer>(lua_rawlen(state, index));
		if (size > 0) {
			for (lua_Integer i = 1; i <= size; ++i) {
				lua_rawgeti(state, index, i);
				text += i > 1 ? ", " : "";
				describe_value(state, -1, text);
				lua_pop(state, 1);
			}
			break;
		}

		vector<string> fields;
		lua_pushnil(state);
		while (lua_next(state, index)) {
			string field = lua_type(state, -2) == LUA_TSTRING ? lua_tostring(state, -2) : "?";
			field += "=";
			describe_value(state, -1, field);
			fields.push_back(field);
			lua_pop(state, 1);
		}
		std::sort(fields.begin(), fields.end());
		text += "{";
		for (size_t i = 0; i < fields.size(); ++i) {
			text += (i > 0 ? ", " : "") + fields[i];
		}
		text += "}";
		break;
	}
	default:
		text += luaL_typename(state, index);
	}
}


/** Append a `name: value' line to text for each property in table, except those whose names start with one of skip */
template <class T>
static void
describe_table(lua_State* state, PropertyTable<T> const& table, T const& object, string& text, vector<string> const& skip = {})
{
	for (auto const& name: table.names()) {
		if (std::any_of(skip.begin(), skip.end(), [&name](string const& prefix) { return name.compare(0, prefix.size(), prefix) == 0; })) {
			continue;
		}
		table.get(state, object, name);
		text += name + ": ";
		describe_value(state, -1, text);
		text += "\n";
		lua_pop(state, 1);
	}
}


static int
open_film(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = make_shared<Film>(boost::filesystem::path(check_string(state, 1)));
		watch_film(state, film);
		auto const notes = film->read_metadata();
		/* As the GUI does when it loads a film */
		if (film->state_version() == 4) {
			lua_warning(state, "This film was created with an old version of DVD-o-matic and may not load correctly in this version.  Please check the film's settings carefully.");
		}
		for (auto const& note: notes) {
			lua_warning(state, note);
		}

		/* As the GUI does after loading; its errors (such as missing files) are in the GUI's jobs panel, not a failure to open */
		auto const before = JobManager::instance()->get();
		JobManager::instance()->add(make_shared<CheckContentJob>(film));
		wait_for_jobs(state, before, true);

		push_film(state, film);
		return 1;
	});
}


/** @return the templates as one string, for errors */
static string
template_list()
{
	auto const names = Config::instance()->templates();
	if (names.empty()) {
		return "there are none";
	}
	string list = "the templates are ";
	for (size_t i = 0; i < names.size(); ++i) {
		list += (i > 0 ? ", " : "") + names[i];
	}
	return list;
}


/** @return the name of an existing template at index; @throw LuaError, starting with what, if it is not one */
static string
check_template(lua_State* state, int index, string const& what)
{
	string name;
	try {
		name = to_string(state, index);
	} catch (PropertyValueError& e) {
		throw LuaError(fmt::format("{}: {}", what, e.what()));
	}
	if (!Config::instance()->existing_template(name)) {
		throw LuaError(fmt::format("{}: no template '{}'; {}", what, name, template_list()));
	}
	return name;
}


static int
new_film(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		boost::filesystem::path const path(check_string(state, 1));
		try {
			check_known_options(state, 2, { "template" });
		} catch (LuaError& e) {
			throw LuaError(fmt::format("dcpomatic.new_film: {}", e.what()));
		}
		optional<string> template_name;
		if (lua_type(state, 2) == LUA_TTABLE) {
			raw_field(state, 2, "template");
			if (!lua_isnil(state, -1)) {
				template_name = check_template(state, -1, "dcpomatic.new_film: option 'template'");
			}
			lua_pop(state, 1);
		} else if (!lua_isnoneornil(state, 2)) {
			throw LuaError("dcpomatic.new_film: argument 2: expected a table of options");
		}

		/* Writing over another film's metadata would lose it */
		if (dcp::filesystem::exists(path / "metadata.xml")) {
			throw LuaError(fmt::format("there is already a film in {}", path.string()));
		}
		auto film = make_shared<Film>(path);
		watch_film(state, film);
		/* As the GUI's New film (dcpomatic.cc) */
		film->use_template(template_name);
		film->set_name(path.filename().generic_string());
		film->write_metadata();
		push_film(state, film);
		return 1;
	});
}


static int
film_get(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		film_properties().get(state, *check_film(state, 1), check_string(state, 2));
		return 1;
	});
}


static int
film_set(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		film_properties().set(state, *check_film(state, 1), check_string(state, 2), 3);
		return 0;
	});
}


static int
film_save(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		check_film(state, 1)->write_metadata();
		return 0;
	});
}


static int
film_content(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		lua_newtable(state);
		int n = 1;
		for (auto content: film->content()) {
			push_content(state, content, film);
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


static int
film_add_content(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		boost::filesystem::path const path(check_string(state, 2));
		/* A folder with an ASSETMAP is a DCP, as the GUI decides when one is dropped on the content list */
		auto content = contains_assetmap(path) ? vector<shared_ptr<Content>>{make_shared<DCPContent>(path)} : content_factory(path);
		if (content.empty()) {
			throw LuaError(fmt::format("could not find any content in {}", path.string()));
		}

		auto const before = JobManager::instance()->get();
		film->examine_and_add_content(content);
		wait_for_jobs(state, before);

		auto const all = film->content();
		if (std::find(all.begin(), all.end(), content.front()) == all.end()) {
			throw LuaError(fmt::format("{} was not added to the film", path.string()));
		}
		push_content(state, content.front(), film);
		return 1;
	});
}


static int
film_remove_content(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		auto const context = check_content(state, 2);
		if (context.film != film) {
			throw LuaError("argument 2: the content is in another film");
		}
		film->remove_content(context.content);
		return 0;
	});
}


/** @return the content of the dcpomatic.Content at index; @throw LuaError, naming method, if it is not in film */
static shared_ptr<Content>
check_own_content(lua_State* state, shared_ptr<Film> film, int index, string const& method)
{
	ContentContext context;
	try {
		context = check_content(state, index);
	} catch (LuaError& e) {
		throw LuaError(fmt::format("film:{}: {}", method, e.what()));
	}
	if (context.film != film) {
		throw LuaError(fmt::format("film:{}: argument {}: the content is in another film", method, index));
	}
	return context.content;
}


static int
film_move_earlier(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		film->move_content_earlier(check_own_content(state, film, 2, "move_earlier"));
		return 0;
	});
}


static int
film_move_later(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		film->move_content_later(check_own_content(state, film, 2, "move_later"));
		return 0;
	});
}


/** film:repeat_content({ c, ... }, n): n more copies of the content, as the GUI's Repeat dialog makes */
static int
film_repeat_content(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		if (lua_type(state, 2) != LUA_TTABLE) {
			throw LuaError("film:repeat_content: argument 2: expected a list of Content");
		}
		ContentList content;
		auto const size = static_cast<lua_Integer>(lua_rawlen(state, 2));
		for (lua_Integer i = 1; i <= size; ++i) {
			lua_rawgeti(state, 2, i);
			content.push_back(check_own_content(state, film, lua_gettop(state), fmt::format("repeat_content: argument 2, item {}", i)));
			lua_pop(state, 1);
		}
		if (content.empty()) {
			throw LuaError("film:repeat_content: argument 2: the list is empty, so there is nothing to repeat (the GUI does nothing then)");
		}

		int number = 0;
		try {
			number = to_int(state, 3);
		} catch (PropertyValueError&) {
		}
		/* The range of the GUI's Repeat dialog */
		if (number < 1 || number > 1024) {
			throw LuaError("film:repeat_content: argument 3: expected 1 to 1024");
		}

		film->repeat_content(content, number);
		return 0;
	});
}


/** film:join({ c, ... }): replace FFmpeg content with one piece that plays it all, as the GUI's Join does.
 *  @return the joined content; the handles of the content that was joined then raise.
 */
static int
film_join(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		string const usage = "film:join: argument 2: expected a list of two or more pieces of content";
		if (lua_type(state, 2) != LUA_TTABLE) {
			throw LuaError(usage);
		}
		ContentList content;
		auto const size = static_cast<lua_Integer>(lua_rawlen(state, 2));
		for (lua_Integer i = 1; i <= size; ++i) {
			lua_rawgeti(state, 2, i);
			auto const item = fmt::format("film:join: argument 2, item {}", i);
			if (!is_content(state, -1)) {
				throw LuaError(fmt::format("{}: expected a Content", item));
			}
			ContentContext context;
			try {
				context = check_content(state, -1);
			} catch (LuaError& e) {
				throw LuaError(fmt::format("{}: {}", item, e.what()));
			}
			lua_pop(state, 1);
			if (context.film != film) {
				throw LuaError(fmt::format("{}: the content is in another film", item));
			}
			auto c = context.content;
			/* The GUI joins only the FFmpeg content of the selection (content_menu.cc) */
			if (!dynamic_pointer_cast<FFmpegContent>(c)) {
				throw LuaError(fmt::format("{}: only content read by FFmpeg (video and sound files) can be joined", item));
			}
			if (std::find(content.begin(), content.end(), c) != content.end()) {
				throw LuaError(fmt::format("{}: the content is in the list twice", item));
			}
			content.push_back(c);
		}
		if (content.size() < 2) {
			throw LuaError(usage);
		}

		shared_ptr<FFmpegContent> joined;
		try {
			joined = make_shared<FFmpegContent>(content);
		} catch (JoinError& e) {
			throw LuaError(fmt::format("film:join: {}", e.what()));
		}

		/* As the GUI does */
		film->remove_content(content);
		auto const before = JobManager::instance()->get();
		film->examine_and_add_content({joined});
		wait_for_jobs(state, before);

		auto const all = film->content();
		if (std::find(all.begin(), all.end(), joined) == all.end()) {
			throw LuaError("film:join: the joined content was not added to the film");
		}
		push_content(state, joined, film);
		return 1;
	});
}


/** film:find_missing(path): look in path (a folder, or the folder of a file) for the film's missing content,
 *  as the GUI's Find missing does.
 *  @return the number of pieces of content that were missing and are not now.
 */
static int
film_find_missing(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		boost::filesystem::path const clue(check_string(state, 2));
		if (!dcp::filesystem::exists(clue)) {
			throw LuaError(fmt::format("film:find_missing: {} does not exist", clue.string()));
		}

		auto missing = [](shared_ptr<const Content> content) {
			auto const status = content_status(content);
			return std::find(status.begin(), status.end(), "missing") != status.end();
		};

		vector<shared_ptr<Content>> before;
		for (auto content: film->content()) {
			if (missing(content)) {
				before.push_back(content);
			}
		}

		dcpomatic::find_missing(film->content(), clue);

		lua_pushinteger(state, std::count_if(before.begin(), before.end(), [&missing](shared_ptr<Content> content) { return !missing(content); }));
		return 1;
	});
}


/** @return the DCP content of the dcpomatic.Content at index; @throw LuaError, naming method, if it is not DCP content in film */
static shared_ptr<DCPContent>
check_own_dcp(lua_State* state, shared_ptr<Film> film, int index, string const& method)
{
	auto dcp = dynamic_pointer_cast<DCPContent>(check_own_content(state, film, index, method));
	if (!dcp) {
		throw LuaError(fmt::format("film:{}: argument {}: the content is not a DCP", method, index));
	}
	return dcp;
}


/** film:copy_dcp_settings_from(c): set the film up as the DCP c, as the content menu's Use DCP settings does */
static int
film_copy_dcp_settings_from(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		copy_dcp_settings_to_film(check_own_dcp(state, film, 2, "copy_dcp_settings_from"), film);
		return 0;
	});
}


/** film:copy_dcp_markers_from(c): replace the film's markers with those of the DCP c, as the content menu's Use DCP markers does */
static int
film_copy_dcp_markers_from(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		copy_dcp_markers_to_film(check_own_dcp(state, film, 2, "copy_dcp_markers_from"), film);
		return 0;
	});
}


void
examine_content(lua_State* state, shared_ptr<Content> content)
{
	auto const before = JobManager::instance()->get();
	JobManager::instance()->add(make_shared<ExamineContentJob>(vector<shared_ptr<Content>>{content}, false));
	wait_for_jobs(state, before);
}


int
content_reexamine(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		ContentContext context;
		try {
			context = check_content(state, 1);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("c:reexamine: {}", e.what()));
		}
		/* As the GUI's Re-examine does */
		examine_content(state, context.content);
		return 0;
	});
}


static int
film_length(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		lua_pushinteger(state, check_film(state, 1)->length().get());
		return 1;
	});
}


/** Push a list of { from=, to= } in DCP ticks, one per reel */
static void
push_reels(lua_State* state, shared_ptr<const Film> film)
{
	lua_newtable(state);
	int n = 1;
	for (auto const& reel: film->reels()) {
		lua_newtable(state);
		lua_pushinteger(state, reel.from.get());
		lua_setfield(state, -2, "from");
		lua_pushinteger(state, reel.to.get());
		lua_setfield(state, -2, "to");
		lua_rawseti(state, -2, n++);
	}
}


static int
film_reels(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		push_reels(state, check_film(state, 1));
		return 1;
	});
}


/** @return the ISDCF name as the DCP panel shows it and "Copy as name" uses it (dcp_panel.cc) */
static string
isdcf_name(shared_ptr<const Film> film)
{
	return film->isdcf_name(true, Config::instance()->add_multiple_open_text_languages_to_isdcf_name());
}


/** @return the DCP name as the DCP panel displays it (dcp_panel.cc), with today's date, as make_dcp
 *  will use; not the folder name when several open-text languages are in the ISDCF name.
 */
static string
dcp_name(shared_ptr<const Film> film)
{
	return film->dcp_name(true, Config::instance()->add_multiple_open_text_languages_to_isdcf_name());
}


/** @return the folder of the last DCP made, with the film's stored ISDCF date, as the GUI's
 *  "Show DCP" opens it (dcpomatic.cc), without making it.
 */
static boost::filesystem::path
dcp_folder(shared_ptr<const Film> film)
{
	return film->dir(film->dcp_name(false), false);
}


static int
film_isdcf_name(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		push_string(state, isdcf_name(check_film(state, 1)));
		return 1;
	});
}


static int
film_dcp_name(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		push_string(state, dcp_name(check_film(state, 1)));
		return 1;
	});
}


static int
film_dcp_folder(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		push_string(state, dcp_folder(check_film(state, 1)).string());
		return 1;
	});
}


/** @return the analysis of the audio of playlist if it has been made, as the audio dialog loads it, or nothing
 *  if there is none or it is in a format so old that the dialog makes it again.
 */
static shared_ptr<AudioAnalysis>
existing_analysis(shared_ptr<const Film> film, shared_ptr<const Playlist> playlist)
{
	auto const path = film->audio_analysis_path(playlist);
	if (!dcp::filesystem::exists(path)) {
		return {};
	}

	try {
		return make_shared<AudioAnalysis>(path);
	} catch (OldFormatError&) {
	} catch (xmlpp::exception&) {
	}
	return {};
}


/** @return the sample peak in dB with the gain the content has now, as the audio panel and the audio dialog show it */
static float
peak_db(shared_ptr<AudioAnalysis> analysis, shared_ptr<const Playlist> playlist)
{
	return linear_to_db(analysis->overall_sample_peak().first.peak) + analysis->gain_correction(playlist);
}


/** @return the `audio peak: ' line of film:describe() for playlist; method is the call that analyses it */
static string
describe_peak(shared_ptr<const Film> film, shared_ptr<const Playlist> playlist, string const& method)
{
	auto analysis = existing_analysis(film, playlist);
	if (!analysis) {
		return fmt::format("audio peak: not analysed yet ({} analyses it)\n", method);
	}
	return fmt::format("audio peak: {:.2f}dB\n", peak_db(analysis, playlist));
}


/** @return a playlist of just content, as the audio panel and the content's audio dialog analyse it */
static shared_ptr<const Playlist>
content_playlist(shared_ptr<const Film> film, shared_ptr<Content> content)
{
	auto playlist = make_shared<Playlist>();
	playlist->add(film, content);
	return playlist;
}


static void push_markers(lua_State* state, shared_ptr<const Film> film);


static int
film_describe(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		string text;
		describe_table(state, film_properties(), *film, text);
		text += fmt::format("length: {}\n", film->length().get());
		push_reels(state, film);
		text += "reels: ";
		describe_value(state, -1, text);
		text += "\n";
		lua_pop(state, 1);
		text += fmt::format("isdcf_name: {}\n", isdcf_name(film));
		text += fmt::format(
			"dcp_name: {} (as displayed, with today's date, which make_dcp uses; not the folder name when several open-text languages are in the ISDCF name)\n",
			dcp_name(film)
			);
		text += fmt::format("dcp_folder: {} (folder of the last DCP made, as Show DCP opens it)\n", dcp_folder(film).string());
		text += fmt::format("best_video_frame_rate: {} (what film:use_best_frame_rate() sets)\n", film->best_video_frame_rate());
		push_markers(state, film);
		text += "markers: ";
		describe_value(state, -1, text);
		text += "\n";
		lua_pop(state, 1);
		/* Only an analysis already made: making one is a job, which film:audio_analysis() runs */
		text += describe_peak(film, film->playlist(), "film:audio_analysis()");

		int index = 1;
		for (auto content: film->content()) {
			ContentContext const context{film, content};
			text += fmt::format("content {}\n", index);
			text += fmt::format("index: {}\n", index);
			text += fmt::format("path: {}\n", content->path(0).string());
			text += fmt::format("full_length: {}\n", content->full_length(film).get());
			text += fmt::format("length_after_trim: {}\n", content->length_after_trim(film).get());
			auto const status = content_status(content);
			text += "status: ";
			for (size_t i = 0; i < status.size(); ++i) {
				text += (i > 0 ? ", " : "") + status[i];
			}
			text += "\n";
			push_user_properties(state, context);
			auto const properties = static_cast<lua_Integer>(lua_rawlen(state, -1));
			for (lua_Integer i = 1; i <= properties; ++i) {
				lua_rawgeti(state, -1, i);
				text += "property: ";
				describe_value(state, -1, text);
				text += "\n";
				lua_pop(state, 1);
			}
			lua_pop(state, 1);
			/* The video fades and the audio processing description are in this table, as they need the film */
			vector<string> skip;
			if (!content->video) {
				skip.push_back("video.");
			} else if (!is_still(content)) {
				skip.push_back("video.length");
			}
			if (!content->audio) {
				skip.push_back("audio.");
			}
			describe_table(state, content_properties(), context, text, skip);
			if (content->video) {
				describe_table(state, video_properties(), *content->video, text);
			}
			if (content->audio) {
				describe_table(state, audio_properties(), *content->audio, text);
				/* Only the gains that are not zero, as c:mapping()[input][output] */
				auto const mapping = content->audio->mapping();
				for (int input = 0; input < mapping.input_channels(); ++input) {
					for (int output = 0; output < mapping.output_channels(); ++output) {
						if (mapping.get(input, output) != 0) {
							text += fmt::format("audio.mapping[{}][{}]: {}\n", input + 1, output + 1, mapping.get(input, output));
						}
					}
				}
				text += describe_peak(film, content_playlist(film, content), "c:audio_analysis()");
			}
			auto ffmpeg = dynamic_pointer_cast<FFmpegContent>(content);
			if (ffmpeg) {
				describe_table(state, ffmpeg_properties(), *ffmpeg, text);
				/* Only colours already found: finding them is a job, which c:subtitle_colours() runs */
				if (ffmpeg->subtitle_stream()) {
					for (auto const& colour: ffmpeg->subtitle_stream()->colours()) {
						text += fmt::format("subtitle colour: {} to {}\n", rgba_to_string(colour.first), rgba_to_string(colour.second));
					}
				}
			}
			auto dcp = dynamic_pointer_cast<DCPContent>(content);
			if (dcp) {
				DCPContext dcp_context{film, dcp, {}};
				describe_table(state, dcp_properties(), dcp_context, text, { "dcp.reference_text" });
				/* The referencing dialog's two text columns, whether or not the DCP has those texts */
				for (auto type: { TextType::OPEN_SUBTITLE, TextType::CLOSED_CAPTION }) {
					text += fmt::format("dcp.reference_text ({}): {}\n", text_type_to_string(type), dcp->reference_text(type));
				}
				text += "cpls: ";
				try {
					push_cpls(state, dcp);
					describe_value(state, -1, text);
					lua_pop(state, 1);
				} catch (std::exception&) {
					/* As the content menu, which lists no CPLs then */
					text += "(the DCP could not be read)";
				}
				text += "\n";
			}
			for (auto part: content->text) {
				text += fmt::format("text part: {}\n", text_type_to_string(part->original_type()));
				describe_table(state, text_properties(), *part, text);
				for (auto font: part->fonts()) {
					text += fmt::format("text.font: {{file={}, id={}}}\n", font->file() ? font->file()->string() : "", font->id());
				}
			}
			++index;
		}

		lua_pushlstring(state, text.c_str(), text.size());
		return 1;
	});
}


/** @return the boolean field key of the table of options at index; false if it is not there, or there is no table */
bool
check_option(lua_State* state, int index, char const* key)
{
	if (lua_isnoneornil(state, index)) {
		return false;
	}
	if (lua_type(state, index) != LUA_TTABLE) {
		throw LuaError(fmt::format("argument {}: expected a table of options", index));
	}

	raw_field(state, index, key);
	bool value = false;
	if (!lua_isnil(state, -1)) {
		try {
			value = to_bool(state, -1);
		} catch (PropertyValueError& e) {
			throw LuaError(fmt::format("option '{}': {}", key, e.what()));
		}
	}
	lua_pop(state, 1);
	return value;
}


/** @throw LuaError if the table of options at index (if there is one) has a key that is not in known */
void
check_known_options(lua_State* state, int index, vector<string> const& known)
{
	if (lua_type(state, index) != LUA_TTABLE) {
		return;
	}

	index = lua_absindex(state, index);
	lua_pushnil(state);
	while (lua_next(state, index)) {
		string const key = lua_type(state, -2) == LUA_TSTRING ? lua_tostring(state, -2) : luaL_typename(state, -2);
		if (std::find(known.begin(), known.end(), key) == known.end()) {
			throw LuaError(fmt::format("unknown option '{}'; the options are {}", key, join_strings(known, ", ")));
		}
		lua_pop(state, 1);
	}
}


static int
film_make_dcp(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		check_known_options(state, 2, { "ignore_hints", "overwrite", "ignore_disk_space" });
		auto const ignore_hints = check_option(state, 2, "ignore_hints");
		auto const overwrite = check_option(state, 2, "overwrite");
		auto const ignore_disk_space = check_option(state, 2, "ignore_disk_space");

		/* The GUI asks about this first (src/tools/dcpomatic.cc) */
		double required = 0;
		double available = 0;
		if (!ignore_disk_space && !film->should_be_enough_disk_space(required, available)) {
			throw LuaError(fmt::format(
				"film:make_dcp: the DCP for this film will take up about {:.1f} GB, and the disk that you are using only has {:.1f} GB available; "
				"pass {{ ignore_disk_space = true }} to make it anyway",
				required, available
				));
		}

		if (!ignore_hints) {
			/* The hints are warnings as they are found and again in the error: that duplication is intended */
			auto const hints = find_hints(state, film);
			if (!hints.empty()) {
				string message = "the DCP was not made because of these hints; pass { ignore_hints = true } to make it anyway:";
				for (auto const& hint: hints) {
					message += "\n* " + hint;
				}
				throw LuaError(message);
			}
		}

		/* As the GUI does, after the hints (src/tools/dcpomatic.cc) */
		if (film->encrypted()) {
			lua_warning(state, strip_markup(
				"You are making an encrypted DCP.  It will not be possible to make KDMs for this DCP unless you have copies of "
				"the <tt>metadata.xml</tt> file within the film and the metadata files within the DCP.\n\n"
				"You should ensure that these files are <span weight=\"bold\" size=\"larger\">BACKED UP</span> "
				"if you want to make KDMs for this film."
				));
		}

		/* make_dcp() does these too, but the DCP's folder name depends on them */
		if (film->name().empty()) {
			film->set_name("DCP");
		}
		film->set_isdcf_date_today();

		auto const dcp_name = film->dcp_name();
		/* An empty name (after filtering), `.' or `..' is the film's directory or its parent */
		if (dcp_name.empty() || dcp_name == "." || dcp_name == "..") {
			throw LuaError(fmt::format("'{}' cannot be used as the name of a DCP folder; set the film's name", dcp_name));
		}

		/* As the GUI does: an existing DCP is replaced only when asked, keeping its assets for re-use */
		auto const dcp_dir = film->dir(dcp_name, false);
		if (dcp::filesystem::exists(dcp_dir)) {
			if (!overwrite) {
				throw LuaError(fmt::format("the DCP folder {} already exists; pass {{ overwrite = true }} to replace it", dcp_dir.string()));
			}
			preserve_assets(dcp_dir, film->assets_path());
			dcp::filesystem::remove_all(dcp_dir);
		}

		/* As the GUI does, save just before making the DCP */
		film->write_metadata();

		auto const before = JobManager::instance()->get();
		/* As the GUI does: content that changed on disk is examined again and the job stops */
		make_dcp(film, TranscodeJob::ChangedBehaviour::EXAMINE_THEN_STOP);
		wait_for_jobs(state, before);
		return 0;
	});
}


/** @return the string field key of the table of options at index, if it is there */
static optional<string>
string_option(lua_State* state, int index, char const* key)
{
	raw_field(state, index, key);
	optional<string> value;
	try {
		value = to_optional_string(state, -1);
	} catch (PropertyValueError& e) {
		lua_pop(state, 1);
		throw LuaError(fmt::format("option '{}': {}", key, e.what()));
	}
	lua_pop(state, 1);
	return value;
}


/** @return the integer field key of the table of options at index, if it is there */
static optional<int>
int_option(lua_State* state, int index, char const* key)
{
	raw_field(state, index, key);
	optional<int> value;
	if (!lua_isnil(state, -1)) {
		try {
			value = to_int(state, -1);
		} catch (PropertyValueError& e) {
			lua_pop(state, 1);
			throw LuaError(fmt::format("option '{}': {}", key, e.what()));
		}
	}
	lua_pop(state, 1);
	return value;
}


/** @throw LuaError unless the argument at index is a table of options */
static void
check_options_table(lua_State* state, int index)
{
	if (lua_type(state, index) != LUA_TTABLE) {
		throw LuaError(fmt::format("argument {}: expected a table of options", index));
	}
}


/** @return the choice called name from choices; @throw LuaError naming it and the choices */
template <class T>
static T const&
find_choice(vector<T> const& choices, string const& name, string const& what)
{
	auto i = std::find_if(choices.begin(), choices.end(), [&name](T const& c) { return name == c.name; });
	if (i == choices.end()) {
		vector<string> names;
		for (auto const& c: choices) {
			names.push_back(c.name);
		}
		throw LuaError(fmt::format("unknown {} '{}'; the {}s are {}", what, name, what, join_strings(names, ", ")));
	}
	return *i;
}


/** @throw LuaError if path exists and overwrite is false, as the GUI asks before it overwrites */
static void
check_overwrite(boost::filesystem::path const& path, bool overwrite)
{
	if (dcp::filesystem::exists(path) && !overwrite) {
		throw LuaError(fmt::format("{} already exists; pass {{ overwrite = true }} to replace it", path.string()));
	}
}


/** Add a TranscodeJob with encoder made from it, as the GUI does, and wait for it */
static void
transcode(lua_State* state, shared_ptr<Film> film, std::function<shared_ptr<FilmEncoder> (shared_ptr<Job>)> encoder)
{
	auto const before = JobManager::instance()->get();
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::EXAMINE_THEN_STOP);
	job->set_encoder(encoder(job));
	JobManager::instance()->add(job);
	wait_for_jobs(state, before);
}


/** @return the files that FFmpegFilmEncoder writes for output, named as its go() and FileEncoderSet name them */
static vector<boost::filesystem::path>
export_file_names(shared_ptr<const Film> film, boost::filesystem::path const& output, bool split_reels)
{
	auto const extension = dcp::filesystem::extension(output);
	auto const stem = dcp::filesystem::change_extension(output, "").string();
	int const files = split_reels ? film->reels().size() : 1;

	vector<boost::filesystem::path> names;
	for (int i = 0; i < files; ++i) {
		auto name = stem;
		if (files > 1) {
			name += fmt::format(dgettext("libdcpomatic2", "_reel{}"), i + 1);
		}
		if (film->three_d()) {
			names.push_back(fmt::format("{}_{}{}", name, dgettext("libdcpomatic2", "L"), extension));
			names.push_back(fmt::format("{}_{}{}", name, dgettext("libdcpomatic2", "R"), extension));
		} else {
			names.push_back(name + extension);
		}
	}
	return names;
}


/** @throw LuaError naming path unless it is absolute, as the export dialogs enable OK only for absolute paths */
static void
check_absolute(boost::filesystem::path const& path, char const* option)
{
	if (!path.is_absolute()) {
		throw LuaError(fmt::format("option '{}': {} is not an absolute path", option, path.string()));
	}
}


/** film:export{ file =, format =, mixdown =, split_reels =, split_streams =, crf =, overwrite = }:
 *  Export video file and Export audio file (dcpomatic.cc, jobs_export_video_file / jobs_export_audio_file).
 *  @return a list of the files written: one, or one per reel with split_reels, each as two (_L and _R) for a 3D film.
 */
static int
film_export(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		try {
			auto film = check_film(state, 1);
			check_options_table(state, 2);
			check_known_options(state, 2, { "file", "format", "mixdown", "split_reels", "split_streams", "crf", "overwrite" });

			struct Choice
			{
				string name;
				ExportFormat format;
				string extension;
			};

			/* The formats of export_video_file_dialog.cc then export_audio_file_dialog.cc, with the extensions they give */
			vector<Choice> const formats = {
				{ "prores-4444", ExportFormat::PRORES_4444, "mov" },
				{ "prores-hq", ExportFormat::PRORES_HQ, "mov" },
				{ "prores-lt", ExportFormat::PRORES_LT, "mov" },
				{ "mp4", ExportFormat::H264_AAC, "mp4" },
				{ "wav-16", ExportFormat::WAV_16, "wav" },
				{ "wav-24", ExportFormat::WAV_24, "wav" },
			};

			auto const format_name = string_option(state, 2, "format");
			if (!format_name) {
				vector<string> names;
				for (auto const& c: formats) {
					names.push_back(c.name);
				}
				throw LuaError(fmt::format("option 'format' is needed; the formats are {}", join_strings(names, ", ")));
			}
			auto const& format = find_choice(formats, *format_name, "format");

			auto file = string_option(state, 2, "file");
			if (!file) {
				throw LuaError("option 'file' is needed");
			}
			check_absolute(*file, "file");
			auto const path = boost::filesystem::path(*file).replace_extension(format.extension);

			struct Mixdown
			{
				string name;
				bool to_stereo;
			};
			vector<Mixdown> const mixdowns = { { "none", false }, { "stereo", true } };
			auto const mixdown = find_choice(mixdowns, string_option(state, 2, "mixdown").get_value_or("none"), "mixdown").to_stereo;

			auto const split_reels = check_option(state, 2, "split_reels");
			auto const split_streams = check_option(state, 2, "split_streams");
			/* The audio dialog has no such option, and passes false */
			if (split_streams && !format_has_video(format.format)) {
				throw LuaError("option 'split_streams' is only for the video formats");
			}

			/* The video dialog enables its quality slider (0 to 51) only for H.264; the audio dialog passes 0 */
			auto crf = int_option(state, 2, "crf");
			if (crf && format.format != ExportFormat::H264_AAC) {
				throw LuaError("option 'crf' is only for format mp4");
			}
			if (crf && (*crf < 0 || *crf > 51)) {
				throw LuaError("option 'crf' must be from 0 (best) to 51 (worst)");
			}
			if (!crf) {
				/* shortcut: ExportConfig's own default, not the value the dialog last remembered, so a script does the same each time */
				crf = format_has_video(format.format) ? 23 : 0;
			}

			auto const names = export_file_names(film, path, split_reels);
			auto const overwrite = check_option(state, 2, "overwrite");
			for (auto const& name: names) {
				check_overwrite(name, overwrite);
			}

			transcode(state, film, [&](shared_ptr<Job> job) {
				return make_shared<FFmpegFilmEncoder>(film, job, path, format.format, mixdown, split_reels, split_streams, *crf);
			});

			lua_createtable(state, static_cast<int>(names.size()), 0);
			int n = 1;
			for (auto const& name: names) {
				push_string(state, name.string());
				lua_rawseti(state, -2, n++);
			}
			return 1;
		} catch (std::exception& e) {
			throw LuaError(fmt::format("film:export: {}", e.what()));
		}
	});
}


/** film:export_subtitles{ file = | dir =, format =, split_reels =, include_font =, overwrite = }:
 *  Export subtitles (dcpomatic.cc, jobs_export_subtitles, and export_subtitles_dialog.cc).
 *  @return the file or folder written to.
 */
static int
film_export_subtitles(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		try {
			auto film = check_film(state, 1);
			check_options_table(state, 2);
			check_known_options(state, 2, { "file", "dir", "format", "split_reels", "include_font", "overwrite" });

			/* As the GUI checks before it shows the dialog */
			int open_subs = 0;
			int open_subs_in_use = 0;
			for (auto content: film->content()) {
				for (auto text: content->text) {
					if (text->type() == TextType::OPEN_SUBTITLE) {
						++open_subs;
						if (text->use()) {
							++open_subs_in_use;
						}
					}
				}
			}
			if (open_subs == 0) {
				throw LuaError("Your project contains no open subtitles, so there is nothing to export.");
			}
			if (open_subs_in_use == 0) {
				throw LuaError("Your project contains open subtitles, but none are being used.  Tick the \"Use as\" checkbox for the subtitles that you want to use.");
			}

			struct Choice
			{
				string name;
				SubtitleFormat format;
				string extension;
			};
			vector<Choice> const formats = {
				{ "xml", SubtitleFormat::XML, "xml" },
				{ "mxf", SubtitleFormat::MXF, "mxf" },
				{ "srt", SubtitleFormat::SRT, "srt" },
			};
			/* The dialog starts at XML for Interop and MXF for SMPTE */
			auto const& format = find_choice(formats, string_option(state, 2, "format").get_value_or(film->interop() ? "xml" : "mxf"), "format");

			auto const split_reels = check_option(state, 2, "split_reels");
			if (split_reels && film->reels().size() > 1) {
				throw LuaError("option 'split_reels' is not offered for a film with more than one reel (the GUI greys it out)");
			}
			auto const include_font = check_option(state, 2, "include_font");
			if (include_font && format.format != SubtitleFormat::XML) {
				throw LuaError("option 'include_font' is only for format xml (MXF always includes the font)");
			}

			/* As the dialog: more than one file goes into a folder, one file has the format's extension */
			auto const file = string_option(state, 2, "file");
			auto const dir = string_option(state, 2, "dir");
			auto const name = film->isdcf_name(true, Config::instance()->add_multiple_open_text_languages_to_isdcf_name());
			boost::filesystem::path output;
			if (split_reels || (format.format == SubtitleFormat::XML && include_font)) {
				if (file || !dir) {
					throw LuaError("this export writes more than one file, so give a folder as option 'dir', not option 'file'");
				}
				check_absolute(*dir, "dir");
				if (!dcp::filesystem::is_directory(*dir)) {
					throw LuaError(fmt::format("option 'dir': {} is not a folder", *dir));
				}
				output = *dir;
				/* With split_reels refused for more than one reel there is one file, named as SubtitleFilmEncoder names it */
				check_overwrite(dcp::filesystem::change_extension(output / name, "." + format.extension), check_option(state, 2, "overwrite"));
			} else {
				if (dir || !file) {
					throw LuaError("this export writes one file, so give it as option 'file', not option 'dir'");
				}
				check_absolute(*file, "file");
				output = boost::filesystem::path(*file).replace_extension(format.extension);
				check_overwrite(output, check_option(state, 2, "overwrite"));
			}

			transcode(state, film, [&](shared_ptr<Job> job) {
				return make_shared<SubtitleFilmEncoder>(film, job, output, name, split_reels, format.format == SubtitleFormat::MXF || include_font, format.format);
			});

			push_string(state, output.string());
			return 1;
		} catch (std::exception& e) {
			throw LuaError(fmt::format("film:export_subtitles: {}", e.what()));
		}
	});
}


static int
film_hints(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto const hints = find_hints(state, check_film(state, 1));
		lua_newtable(state);
		int n = 1;
		for (auto const& hint: hints) {
			push_string(state, hint);
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


/** Every marker, in libdcp's order */
static vector<dcp::Marker> const all_markers = {
	dcp::Marker::FFOC, dcp::Marker::LFOC, dcp::Marker::FFTC, dcp::Marker::LFTC, dcp::Marker::FFOI, dcp::Marker::LFOI,
	dcp::Marker::FFEC, dcp::Marker::LFEC, dcp::Marker::FFMC, dcp::Marker::LFMC, dcp::Marker::FFOB, dcp::Marker::LFOB
};


/** The markers that the Markers dialog and the markers panel offer to set, in their order (src/wx/markers.cc) */
static vector<dcp::Marker> const editable_markers = {
	dcp::Marker::FFOB, dcp::Marker::LFOB, dcp::Marker::FFTC, dcp::Marker::LFTC, dcp::Marker::FFOI,
	dcp::Marker::LFOI, dcp::Marker::FFEC, dcp::Marker::LFEC, dcp::Marker::FFMC, dcp::Marker::LFMC
};


/** @return the names of markers, separated by commas */
static string
marker_names(vector<dcp::Marker> const& markers)
{
	string names;
	for (auto marker: markers) {
		names += (names.empty() ? "" : ", ") + dcp::marker_to_string(marker);
	}
	return names;
}


/** @return the marker named (as dcp::marker_to_string names it) by the string at index, which must be one of allowed;
 *  @throw LuaError naming method.
 */
static dcp::Marker
check_marker(lua_State* state, int index, string const& method, vector<dcp::Marker> const& allowed)
{
	string name;
	try {
		name = to_string(state, index);
	} catch (PropertyValueError& e) {
		throw LuaError(fmt::format("film:{}: argument {}: {}", method, index, e.what()));
	}
	/* dcp::marker_from_string asserts on a name it does not know */
	for (auto marker: allowed) {
		if (dcp::marker_to_string(marker) == name) {
			return dcp::marker_from_string(name);
		}
	}
	for (auto marker: all_markers) {
		if (dcp::marker_to_string(marker) == name) {
			throw LuaError(fmt::format(
				"film:{}: argument {}: {} is set by DCP-o-matic itself, and the GUI does not offer to set it; the markers that can be set are {}",
				method, index, name, marker_names(allowed)
				));
		}
	}
	throw LuaError(fmt::format("film:{}: argument {}: unknown marker '{}'; the markers are {}", method, index, name, marker_names(allowed)));
}


/** Push a table of marker name -> time in ticks */
static void
push_markers(lua_State* state, shared_ptr<const Film> film)
{
	lua_newtable(state);
	for (auto const& marker: film->markers()) {
		lua_pushinteger(state, marker.second.get());
		lua_setfield(state, -2, dcp::marker_to_string(marker.first).c_str());
	}
}


static int
film_markers(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		push_markers(state, check_film(state, 1));
		return 1;
	});
}


/** film:set_marker(name, ticks), as the Markers dialog sets one (markers_dialog.cc) */
static int
film_set_marker(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		auto const marker = check_marker(state, 2, "set_marker", editable_markers);
		dcpomatic::DCPTime time;
		try {
			time = to_dcp_time(state, 3);
		} catch (PropertyValueError& e) {
			throw LuaError(fmt::format("film:set_marker: argument 3: {}", e.what()));
		}
		if (time < dcpomatic::DCPTime()) {
			throw LuaError("film:set_marker: argument 3: expected 0 or more ticks");
		}
		/* The DCP panel disables Markers for these (dcp_panel.cc), and their DCPs get no markers (reel_writer.cc) */
		if (film->interop()) {
			throw LuaError("film:set_marker: the film is Interop, whose DCPs have no markers (the GUI disables Markers then)");
		}
		if (film->smpte_profile() == dcp::Profile::SMPTE_A) {
			throw LuaError("film:set_marker: the film's SMPTE profile is SMPTE A, whose DCPs have no markers (the GUI disables Markers then)");
		}
		/* As the dialog does: a time at or past the end is moved to the last frame (a film is at least 1 second long) */
		auto const vfr = film->video_frame_rate();
		if (time >= film->length()) {
			time = film->length() - dcpomatic::DCPTime::from_frames(1, vfr);
			lua_warning(state, fmt::format("film:set_marker: {} is at or past the end of the film, so it was moved to the last frame, {} ticks", dcp::marker_to_string(marker), time.get()));
		}
		film->set_marker(marker, time);
		return 0;
	});
}


static int
film_unset_marker(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		/* Any marker, as the markers panel removes any that it shows */
		film->unset_marker(check_marker(state, 2, "unset_marker", all_markers));
		return 0;
	});
}


/** film:duplicate(dir[, { overwrite = true }]): copy the saved film into dir, as File -> Duplicate does (dcpomatic.cc).
 *  @return the copy.
 */
static int
film_duplicate(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		boost::filesystem::path path;
		bool overwrite = false;
		try {
			path = check_string(state, 2);
			check_known_options(state, 3, { "overwrite" });
			overwrite = check_option(state, 3, "overwrite");
		} catch (LuaError& e) {
			throw LuaError(fmt::format("film:duplicate: {}", e.what()));
		}
		/* The copy is made from the saved metadata; the GUI offers to save first */
		if (film->dirty()) {
			throw LuaError("film:duplicate: the film has unsaved changes; call film:save() first (the GUI offers to save them)");
		}
		/* As FilmNameLocationDialog::check_path, which asks before it uses a directory that is not empty */
		if (dcp::filesystem::is_regular_file(path)) {
			throw LuaError(fmt::format("film:duplicate: argument 2: {} already exists as a file, so you cannot use it for a film.", path.string()));
		}
		boost::system::error_code ec;
		if (boost::filesystem::equivalent(path, *film->directory(), ec)) {
			throw LuaError(fmt::format("film:duplicate: argument 2: {} is the film's own directory", path.string()));
		}
		if (dcp::filesystem::is_directory(path) && !dcp::filesystem::is_empty(path) && !overwrite) {
			throw LuaError(fmt::format("film:duplicate: the directory {} already exists and is not empty; pass {{ overwrite = true }} to use it", path.string()));
		}

		auto copy = make_shared<Film>(path);
		watch_film(state, copy);
		copy->copy_from(film, [](float) {});
		copy->set_name(path.filename().generic_string());
		copy->write_metadata();
		push_film(state, copy);
		return 1;
	});
}


/** @return the template name at index, which must not be empty; @throw LuaError starting with what */
static string
check_template_name(lua_State* state, int index, string const& what)
{
	string name;
	try {
		name = to_string(state, index);
	} catch (PropertyValueError& e) {
		throw LuaError(fmt::format("{}: argument {}: {}", what, index, e.what()));
	}
	/* The GUI's dialogs need a name */
	if (name.empty()) {
		throw LuaError(fmt::format("{}: argument {}: template names must not be empty", what, index));
	}
	return name;
}


/** film:save_as_template(name[, { overwrite = true }]), as Save as template does with a new or existing name */
static int
film_save_as_template(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		auto const name = check_template_name(state, 2, "film:save_as_template");
		bool overwrite = false;
		try {
			check_known_options(state, 3, { "overwrite" });
			overwrite = check_option(state, 3, "overwrite");
		} catch (LuaError& e) {
			throw LuaError(fmt::format("film:save_as_template: {}", e.what()));
		}
		/* The GUI asks before it overwrites (save_template_dialog.cc) */
		if (Config::instance()->existing_template(name) && !overwrite) {
			throw LuaError(fmt::format("film:save_as_template: there is already a template called '{}'; pass {{ overwrite = true }} to replace it", name));
		}
		Config::instance()->save_template(film, name);
		return 0;
	});
}


/** film:save_as_default_template(), as Save as template does with "Default template" */
static int
film_save_as_default_template(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		Config::instance()->save_default_template(check_film(state, 1));
		return 0;
	});
}


static int
templates(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		lua_newtable(state);
		int n = 1;
		for (auto const& name: Config::instance()->templates()) {
			push_string(state, name);
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


/** dcpomatic.rename_template(old, new), as the Templates dialog's Rename does */
static int
rename_template(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto const old_name = check_template(state, 1, "dcpomatic.rename_template: argument 1");
		auto const new_name = check_template_name(state, 2, "dcpomatic.rename_template");
		Config::instance()->rename_template(old_name, new_name);
		return 0;
	});
}


/** dcpomatic.delete_template(name), as the Templates dialog's Remove does */
static int
delete_template(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		Config::instance()->delete_template(check_template(state, 1, "dcpomatic.delete_template: argument 1"));
		return 0;
	});
}


/** film:use_best_frame_rate(), as the DCP panel's Use best does (dcp_panel.cc).
 *  @return the frame rate now set.
 */
static int
film_use_best_frame_rate(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		if (auto const reason = dcpomatic::film::why_frame_rate_is_fixed(*film)) {
			throw LuaError(fmt::format("film:use_best_frame_rate: {}, so its frame rate cannot change", *reason));
		}
		film->set_video_frame_rate(film->best_video_frame_rate(), false);
		lua_pushinteger(state, film->video_frame_rate());
		return 1;
	});
}


/** film:send_to_tms(), as Jobs -> Send DCP to TMS does; waits for the upload */
static int
film_send_to_tms(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto film = check_film(state, 1);
		/* The menu item needs a CPL */
		if (film->cpls().empty()) {
			throw LuaError("film:send_to_tms: the film has no DCP to send; make one first (the GUI offers Send DCP to TMS only then)");
		}
		auto const before = JobManager::instance()->get();
		film->send_dcp_to_tms();
		try {
			wait_for_jobs(state, before);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("film:send_to_tms: {}", e.what()));
		}
		return 0;
	});
}


/** Push the levels of the audio of playlist, analysing it first and waiting if it has not been, as the audio dialog
 *  does (audio_dialog.cc).  Every level is in dB with the gain correction the dialog adds.
 *  @param from_zero true for the whole film, as the dialog passes when it shows the film's audio.
 *  @param method name of the call for errors.
 */
static void
push_audio_analysis(lua_State* state, shared_ptr<Film> film, shared_ptr<const Playlist> playlist, bool from_zero, string const& method)
{
	auto analysis = existing_analysis(film, playlist);
	if (!analysis) {
		auto const before = JobManager::instance()->get();
		boost::signals2::connection connection;
		JobManager::instance()->analyse_audio(film, playlist, from_zero, connection, [](Job::Result) {});
		try {
			wait_for_jobs(state, before);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("{}: {}", method, e.what()));
		}
		analysis = existing_analysis(film, playlist);
		if (!analysis) {
			/* As the dialog says when the job leaves no analysis */
			throw LuaError(fmt::format("{}: could not analyse audio", method));
		}
	}

	auto const gain = analysis->gain_correction(playlist);
	auto const overall = analysis->overall_sample_peak();

	lua_newtable(state);
	lua_pushnumber(state, peak_db(analysis, playlist));
	lua_setfield(state, -2, "peak_db");
	lua_pushinteger(state, overall.first.time.get());
	lua_setfield(state, -2, "peak_time");
	push_string(state, short_audio_channel_name(overall.second));
	lua_setfield(state, -2, "peak_channel");
	if (auto true_peak = analysis->overall_true_peak()) {
		lua_pushnumber(state, linear_to_db(*true_peak) + gain);
		lua_setfield(state, -2, "true_peak_db");
	}
	if (auto loudness = analysis->integrated_loudness()) {
		lua_pushnumber(state, *loudness + gain);
		lua_setfield(state, -2, "integrated_loudness");
	}
	if (auto range = analysis->loudness_range()) {
		lua_pushnumber(state, *range + gain);
		lua_setfield(state, -2, "loudness_range");
	}
	if (auto leqm = analysis->leqm()) {
		lua_pushnumber(state, *leqm + gain);
		lua_setfield(state, -2, "leqm");
	}

	auto const sample_peaks = analysis->sample_peak();
	auto const true_peaks = analysis->true_peak();
	lua_newtable(state);
	for (size_t i = 0; i < sample_peaks.size(); ++i) {
		lua_newtable(state);
		push_string(state, short_audio_channel_name(i));
		lua_setfield(state, -2, "name");
		lua_pushnumber(state, linear_to_db(sample_peaks[i].peak) + gain);
		lua_setfield(state, -2, "peak_db");
		lua_pushinteger(state, sample_peaks[i].time.get());
		lua_setfield(state, -2, "peak_time");
		if (i < true_peaks.size()) {
			lua_pushnumber(state, linear_to_db(true_peaks[i]) + gain);
			lua_setfield(state, -2, "true_peak_db");
		}
		lua_rawseti(state, -2, static_cast<lua_Integer>(i + 1));
	}
	lua_setfield(state, -2, "channels");
}


/** c:audio_analysis(): the levels of the content's audio, as its audio dialog shows them */
int
content_audio_analysis(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		ContentContext context;
		try {
			context = check_content(state, 1);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("c:audio_analysis: {}", e.what()));
		}
		if (!context.content->audio) {
			throw LuaError("c:audio_analysis: the content has no audio (the GUI shows no levels for it)");
		}
		push_audio_analysis(state, context.film, content_playlist(context.film, context.content), false, "c:audio_analysis");
		return 1;
	});
}


/** film:audio_analysis(): the levels of the film's audio, as the DCP panel's audio dialog shows them */
static int
film_audio_analysis(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		shared_ptr<Film> film;
		try {
			film = check_film(state, 1);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("film:audio_analysis: {}", e.what()));
		}
		push_audio_analysis(state, film, film->playlist(), true, "film:audio_analysis");
		return 1;
	});
}


/** dcpomatic.gain_for_fader_change(from, to [, processor]): the gain in dB that the gain calculator gives for content
 *  meant to play at fader from that sounds right at fader to, on processor (an id; the calculator's first by default).
 */
static int
gain_for_fader_change(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto number = [state](int index) {
			try {
				return to_double(state, index);
			} catch (PropertyValueError& e) {
				throw LuaError(fmt::format("dcpomatic.gain_for_fader_change: argument {}: {}", index, e.what()));
			}
		};
		auto const from = number(1);
		auto const to = number(2);

		/* The calculator selects the first processor to begin with */
		auto processor = CinemaSoundProcessor::all().front();
		if (!lua_isnoneornil(state, 3)) {
			string id;
			try {
				id = check_string(state, 3);
			} catch (LuaError& e) {
				throw LuaError(fmt::format("dcpomatic.gain_for_fader_change: {}", e.what()));
			}
			processor = CinemaSoundProcessor::from_id(id);
			if (!processor) {
				string all;
				for (auto i: CinemaSoundProcessor::all()) {
					all += fmt::format("{}{} ({})", all.empty() ? "" : ", ", i->id(), i->name());
				}
				throw LuaError(fmt::format("dcpomatic.gain_for_fader_change: argument 3: unknown sound processor '{}'; the processors are {}", id, all));
			}
		}

		lua_pushnumber(state, processor->db_for_fader_change(from, to));
		return 1;
	});
}


static int
film_gc(lua_State* state)
{
	static_cast<shared_ptr<Film>*>(lua_touserdata(state, 1))->~shared_ptr<Film>();
	return 0;
}


void
register_film(lua_State* state)
{
	static luaL_Reg const methods[] = {
		{ "get", film_get },
		{ "set", film_set },
		{ "save", film_save },
		{ "content", film_content },
		{ "add_content", film_add_content },
		{ "remove_content", film_remove_content },
		{ "move_earlier", film_move_earlier },
		{ "move_later", film_move_later },
		{ "repeat_content", film_repeat_content },
		{ "join", film_join },
		{ "find_missing", film_find_missing },
		{ "copy_dcp_settings_from", film_copy_dcp_settings_from },
		{ "copy_dcp_markers_from", film_copy_dcp_markers_from },
		{ "describe", film_describe },
		{ "hints", film_hints },
		{ "make_dcp", film_make_dcp },
		{ "export", film_export },
		{ "export_subtitles", film_export_subtitles },
		{ "length", film_length },
		{ "reels", film_reels },
		{ "isdcf_name", film_isdcf_name },
		{ "dcp_name", film_dcp_name },
		{ "dcp_folder", film_dcp_folder },
		{ "markers", film_markers },
		{ "set_marker", film_set_marker },
		{ "unset_marker", film_unset_marker },
		{ "duplicate", film_duplicate },
		{ "save_as_template", film_save_as_template },
		{ "save_as_default_template", film_save_as_default_template },
		{ "use_best_frame_rate", film_use_best_frame_rate },
		{ "send_to_tms", film_send_to_tms },
		{ "audio_analysis", film_audio_analysis },
		{ nullptr, nullptr }
	};
	register_metatable(state, film_metatable, methods, film_gc);

	static luaL_Reg const functions[] = {
		{ "open_film", open_film },
		{ "new_film", new_film },
		{ "templates", templates },
		{ "rename_template", rename_template },
		{ "delete_template", delete_template },
		{ "gain_for_fader_change", gain_for_fader_change },
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

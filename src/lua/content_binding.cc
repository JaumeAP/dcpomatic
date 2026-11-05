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


/** @file  src/lua/content_binding.cc
 *  @brief The dcpomatic.Content userdata: c:get(name [, text_type]), c:set(name, value [, text_type]),
 *  c:path(), c:index(), c:reexamine(), c:audio_analysis(), and the read-only facts c:full_length(), c:length_after_trim() (ticks),
 *  c:properties() and c:status(); the GUI's content operations c:subtitle_streams(), c:subtitle_colours(),
 *  c:set_subtitle_colour(), c:fonts(), c:set_font_file(), c:auto_crop{} and c:copy_settings_from(); and for DCP content
 *  c:cpls(), c:add_ov() and c:add_kdm().
 *
 *  Names of the settings of the video, audio and text of the content start with `video.',
 *  `audio.' and `text.'; the content's own (position, trims) have no prefix.  Content with more
 *  than one text (subtitles and closed captions) needs the original type of the one meant.
 */


#include "content_binding.h"
#include "warnings.h"
#include "lib/audio_content.h"
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/content.h"
#include "lib/dcp_content.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/examine_ffmpeg_subtitles_job.h"
#include "lib/exceptions.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/film.h"
#include "lib/font.h"
#include "lib/guess_crop.h"
#include "lib/job_manager.h"
#include "lib/rgba.h"
#include "lib/text_content.h"
#include "lib/user_property.h"
#include "lib/util.h"
#include "lib/video_content.h"
#include <dcp/cpl.h>
#include <dcp/encrypted_kdm.h>
#include <dcp/filesystem.h>
#include <dcp/search.h>
#include <dcp/util.h>
#include <fmt/ranges.h>
#include <algorithm>
#include <new>


using std::dynamic_pointer_cast;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;


static char const* const content_metatable = "dcpomatic.Content";


/** What a dcpomatic.Content userdata holds */
struct ContentHandle
{
	shared_ptr<Content> content;
	weak_ptr<Film> film;
};


void
push_content(lua_State* state, shared_ptr<Content> content, shared_ptr<Film> film)
{
	new (lua_newuserdatauv(state, sizeof(ContentHandle), 0)) ContentHandle{content, film};
	luaL_setmetatable(state, content_metatable);
}


bool
is_content(lua_State* state, int index)
{
	return luaL_testudata(state, index, content_metatable);
}


ContentContext
check_content(lua_State* state, int index)
{
	auto handle = static_cast<ContentHandle*>(luaL_testudata(state, index, content_metatable));
	if (!handle) {
		throw LuaError(fmt::format("argument {}: expected a Content", index));
	}

	auto film = handle->film.lock();
	auto const all = film ? film->content() : ContentList();
	if (std::find(all.begin(), all.end(), handle->content) == all.end()) {
		throw LuaError("content was removed from the film");
	}

	return { film, handle->content };
}


/** @return the text of content that a text.* property or a text method is for: the one whose original
 *  type is the selector (`open-subtitle', `closed-caption', ...), as the GUI's text tabs choose, or
 *  the only one if there is no selector.
 *  @throw LuaError, without the name of the property or method, if there is no such text.
 */
static shared_ptr<TextContent>
text_part(shared_ptr<Content> content, optional<string> selector)
{
	if (!selector) {
		if (content->text.empty()) {
			throw LuaError("the content has no text");
		}
		if (content->text.size() > 1) {
			string types;
			for (auto text: content->text) {
				types += (types.empty() ? "" : ", ") + text_type_to_string(text->original_type());
			}
			throw LuaError(fmt::format("the content has several texts; name one of {}", types));
		}
		return content->text.front();
	}

	TextType type;
	try {
		type = parse(*selector, string_to_text_type);
	} catch (PropertyValueError& e) {
		throw LuaError(fmt::format("text type {}", e.what()));
	}
	auto text = content->text_of_original_type(type);
	if (!text) {
		throw LuaError(fmt::format("the content has no {} text", *selector));
	}
	return text;
}


/** @return content as FFmpegContent; @throw LuaError if it is not */
static shared_ptr<FFmpegContent>
ffmpeg_part(shared_ptr<Content> content)
{
	auto ffmpeg = dynamic_pointer_cast<FFmpegContent>(content);
	if (!ffmpeg) {
		throw LuaError("the content is not read by FFmpeg (only video and sound files are)");
	}
	return ffmpeg;
}


/** @return content as DCPContent; @throw LuaError if it is not */
static shared_ptr<DCPContent>
dcp_part(shared_ptr<Content> content)
{
	auto dcp = dynamic_pointer_cast<DCPContent>(content);
	if (!dcp) {
		throw LuaError("the content is not a DCP");
	}
	return dcp;
}


/** Call f(table, object) with the property table that name belongs to, by its prefix, and the part of the content it is for.
 *  @param selector Original type of the text that a text.* property or dcp.reference_text is for.
 */
template <class F>
static void
with_table(ContentContext& context, string const& name, optional<string> selector, F f)
{
	auto has_prefix = [&name](string const& prefix) { return name.compare(0, prefix.size(), prefix) == 0; };
	auto content = context.content;

	if (selector && !has_prefix("text.") && name != "dcp.reference_text") {
		throw LuaError(fmt::format("property '{}': only text.* properties and dcp.reference_text take a text type", name));
	}

	if (content_properties().has(name)) {
		/* The content's own, and the video fades, which need the film to convert ticks to frames */
		f(content_properties(), context);
	} else if (has_prefix("video.")) {
		if (!content->video) {
			throw LuaError(fmt::format("property '{}': the content has no video", name));
		}
		f(video_properties(), *content->video);
	} else if (has_prefix("audio.")) {
		if (!content->audio) {
			throw LuaError(fmt::format("property '{}': the content has no audio", name));
		}
		f(audio_properties(), *content->audio);
	} else if (has_prefix("text.")) {
		shared_ptr<TextContent> text;
		try {
			text = text_part(content, selector);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("property '{}': {}", name, e.what()));
		}
		f(text_properties(), *text);
	} else if (has_prefix("ffmpeg.")) {
		shared_ptr<FFmpegContent> ffmpeg;
		try {
			ffmpeg = ffmpeg_part(content);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("property '{}': {}", name, e.what()));
		}
		f(ffmpeg_properties(), *ffmpeg);
	} else if (has_prefix("dcp.")) {
		shared_ptr<DCPContent> dcp;
		try {
			dcp = dcp_part(content);
		} catch (LuaError& e) {
			throw LuaError(fmt::format("property '{}': {}", name, e.what()));
		}
		DCPContext dcp_context{context.film, dcp, selector};
		f(dcp_properties(), dcp_context);
	} else {
		f(content_properties(), context);
	}
}


static int
content_get(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto context = check_content(state, 1);
		auto const name = check_string(state, 2);
		auto const selector = lua_isnoneornil(state, 3) ? optional<string>() : check_string(state, 3);
		with_table(context, name, selector, [state, &name](auto const& table, auto& object) { table.get(state, object, name); });
		return 1;
	});
}


static int
content_set(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto context = check_content(state, 1);
		auto const name = check_string(state, 2);
		auto const selector = lua_isnoneornil(state, 4) ? optional<string>() : check_string(state, 4);
		with_table(context, name, selector, [state, &name](auto const& table, auto& object) { table.set(state, object, name, 3); });
		return 0;
	});
}


static int
content_path(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		push_string(state, check_content(state, 1).content->path(0).string());
		return 1;
	});
}


/** @return the number of the content in the film's list, from 1 */
static int
content_index(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto const context = check_content(state, 1);
		auto const all = context.film->content();
		lua_pushinteger(state, std::find(all.begin(), all.end(), context.content) - all.begin() + 1);
		return 1;
	});
}


static int
content_full_length(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto const context = check_content(state, 1);
		lua_pushinteger(state, context.content->full_length(context.film).get());
		return 1;
	});
}


static int
content_length_after_trim(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		auto const context = check_content(state, 1);
		lua_pushinteger(state, context.content->length_after_trim(context.film).get());
		return 1;
	});
}


static char const*
category_name(UserProperty::Category category)
{
	switch (category) {
	case UserProperty::GENERAL:
		return "general";
	case UserProperty::VIDEO:
		return "video";
	case UserProperty::AUDIO:
		return "audio";
	case UserProperty::LENGTH:
		return "length";
	}
	DCPOMATIC_ASSERT(false);
	return "";
}


void
push_user_properties(lua_State* state, ContentContext const& context)
{
	lua_newtable(state);
	int n = 1;
	for (auto const& property: context.content->user_properties(context.film)) {
		lua_newtable(state);
		lua_pushstring(state, category_name(property.category));
		lua_setfield(state, -2, "category");
		push_optional_string(state, property.sub_heading);
		lua_setfield(state, -2, "sub_heading");
		push_string(state, property.key);
		lua_setfield(state, -2, "name");
		push_string(state, property.value);
		lua_setfield(state, -2, "value");
		push_string(state, property.unit);
		lua_setfield(state, -2, "unit");
		lua_rawseti(state, -2, n++);
	}
}


static int
content_properties_method(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		push_user_properties(state, check_content(state, 1));
		return 1;
	});
}


vector<string>
content_status(shared_ptr<const Content> content)
{
	/* The same checks, in the same order, as the content list (content_panel.cc) */
	vector<string> status;
	if (!paths_exist(content->paths()) || !paths_exist(content->font_paths())) {
		status.push_back("missing");
	}
	auto dcp = dynamic_pointer_cast<const DCPContent>(content);
	if (dcp && dcp->needs_kdm()) {
		status.push_back("needs KDM");
	}
	if (dcp && dcp->needs_assets()) {
		status.push_back("needs OV");
	}
	return status;
}


static int
content_status_method(lua_State* state)
{
	return lua_protect(state, [](lua_State* state) {
		lua_newtable(state);
		int n = 1;
		for (auto const& s: content_status(check_content(state, 1).content)) {
			push_string(state, s);
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


/** As lua_protect, with name (`c:fonts') before the message of any error */
template <class F>
static int
content_method(lua_State* state, char const* name, F f)
{
	return lua_protect(state, [name, f](lua_State* state) {
		try {
			return f(state);
		} catch (std::exception& e) {
			throw LuaError(fmt::format("{}: {}", name, e.what()));
		}
	});
}


/** @return the text selector argument at index, if there is one */
static optional<string>
check_selector(lua_State* state, int index)
{
	return lua_isnoneornil(state, index) ? optional<string>() : check_string(state, index);
}


/** c:subtitle_streams(): a list of { id=, name= }, the choices of the text panel's stream list */
static int
content_subtitle_streams(lua_State* state)
{
	return content_method(state, "c:subtitle_streams", [](lua_State* state) {
		auto ffmpeg = ffmpeg_part(check_content(state, 1).content);
		lua_newtable(state);
		int n = 1;
		for (auto stream: ffmpeg->subtitle_streams()) {
			lua_newtable(state);
			push_string(state, stream->identifier());
			lua_setfield(state, -2, "id");
			push_string(state, stream->name);
			lua_setfield(state, -2, "name");
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


string
rgba_to_string(RGBA colour)
{
	return fmt::format("{:02X}{:02X}{:02X}{:02X}", colour.r, colour.g, colour.b, colour.a);
}


static RGBA
check_rgba(lua_State* state, int index)
{
	auto const text = lua_type(state, index) == LUA_TSTRING ? string(lua_tostring(state, index)) : string();
	if (text.size() != 8 || text.find_first_not_of("0123456789abcdefABCDEF") != string::npos) {
		throw LuaError(fmt::format("argument {}: expected a colour as 8 hex digits RRGGBBAA, such as 'FF000080'", index));
	}
	auto const value = std::stoul(text, nullptr, 16);
	return RGBA((value >> 24) & 0xff, (value >> 16) & 0xff, (value >> 8) & 0xff, value & 0xff);
}


/** @return the subtitle stream of the content at index, with its colours found, as the subtitle appearance dialog finds them */
static std::pair<shared_ptr<FFmpegContent>, shared_ptr<FFmpegSubtitleStream>>
subtitle_stream_with_colours(lua_State* state, int index)
{
	auto ffmpeg = ffmpeg_part(check_content(state, index).content);
	auto stream = ffmpeg->subtitle_stream();
	if (!stream) {
		throw LuaError("the content has no subtitle stream");
	}
	/* When the text panel offers the Appearance button (text_panel.cc) */
	auto text = ffmpeg->only_text();
	if (!text->use() || !is_open(text->type())) {
		throw LuaError("the subtitles must be used as open subtitles or open captions (text.use and text.type), as the GUI's Appearance button needs");
	}
	if (stream->colours().empty()) {
		auto const before = JobManager::instance()->get();
		JobManager::instance()->add(make_shared<ExamineFFmpegSubtitlesJob>(ffmpeg));
		wait_for_jobs(state, before);
	}
	return { ffmpeg, stream };
}


/** c:subtitle_colours(): a list of { from=, to= } (RRGGBBAA), the colours of the bitmap subtitles and what each becomes */
static int
content_subtitle_colours(lua_State* state)
{
	return content_method(state, "c:subtitle_colours", [](lua_State* state) {
		auto const stream = subtitle_stream_with_colours(state, 1).second;
		lua_newtable(state);
		int n = 1;
		for (auto const& colour: stream->colours()) {
			lua_newtable(state);
			push_string(state, rgba_to_string(colour.first));
			lua_setfield(state, -2, "from");
			push_string(state, rgba_to_string(colour.second));
			lua_setfield(state, -2, "to");
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


/** c:set_subtitle_colour(from, to): show a colour of the bitmap subtitles as another, as the subtitle appearance dialog does */
static int
content_set_subtitle_colour(lua_State* state)
{
	return content_method(state, "c:set_subtitle_colour", [](lua_State* state) {
		auto const from = check_rgba(state, 2);
		auto const to = check_rgba(state, 3);
		auto const ffmpeg_and_stream = subtitle_stream_with_colours(state, 1);
		auto const colours = ffmpeg_and_stream.second->colours();
		if (colours.find(from) == colours.end()) {
			throw LuaError(fmt::format("argument 2: {} is not a colour of the subtitles", rgba_to_string(from)));
		}
		ffmpeg_and_stream.second->set_colour(from, to);
		ffmpeg_and_stream.first->signal_subtitle_stream_changed();
		return 0;
	});
}


/** c:fonts([text_type]): a list of { id=, file= } for the fonts of a text, as the fonts dialog lists them; file is nil when unset */
static int
content_fonts(lua_State* state)
{
	return content_method(state, "c:fonts", [](lua_State* state) {
		auto text = text_part(check_content(state, 1).content, check_selector(state, 2));
		lua_newtable(state);
		int n = 1;
		for (auto font: text->fonts()) {
			lua_newtable(state);
			push_string(state, font->id());
			lua_setfield(state, -2, "id");
			push_optional_string(state, font->file() ? font->file()->string() : optional<string>());
			lua_setfield(state, -2, "file");
			lua_rawseti(state, -2, n++);
		}
		return 1;
	});
}


/** c:set_font_file(id, path [, text_type]): use a font file for a font of a text, as the fonts dialog does */
static int
content_set_font_file(lua_State* state)
{
	return content_method(state, "c:set_font_file", [](lua_State* state) {
		auto const content = check_content(state, 1).content;
		auto text = text_part(content, check_selector(state, 4));
		/* When the text panel offers the Fonts button (text_panel.cc): not for what it counts as bitmap subtitles, counted as there */
		auto const dcp = dynamic_pointer_cast<const DCPContent>(content);
		auto const dcp_subtitle = dynamic_pointer_cast<const DCPSubtitleContent>(content);
		if (dynamic_pointer_cast<const FFmpegContent>(content) || (dcp && dcp->text_has_bitmaps(text->original_type())) || (dcp_subtitle && dcp_subtitle->has_bitmaps())) {
			throw LuaError("the GUI sets no fonts for bitmap subtitles or for subtitles inside a video file, which it counts as bitmaps");
		}
		if (!is_open(text->type())) {
			throw LuaError("fonts can be set only for open subtitles and open captions, as in the GUI");
		}
		if (text->fonts().empty()) {
			throw LuaError("the text has no fonts");
		}
		auto const id = check_string(state, 2);
		boost::filesystem::path const file(check_string(state, 3));
		auto font = text->get_font(id);
		if (!font) {
			vector<string> ids;
			for (auto f: text->fonts()) {
				ids.push_back(fmt::format("'{}'", f->id()));
			}
			throw LuaError(fmt::format("argument 2: no font '{}'; the fonts are {}", id, fmt::join(ids, ", ")));
		}
		/* The GUI's file dialogs only give files that exist */
		if (!dcp::filesystem::exists(file)) {
			throw LuaError(fmt::format("argument 3: {} does not exist", file.string()));
		}
		font->set_file(file);
		return 0;
	});
}


/** c:auto_crop{ time = ticks }: crop by brightness at a time in the content, with the threshold of the preferences, as the
 *  Auto-crop dialog does when OK is pressed.
 *  @return the crop, as { left=, right=, top=, bottom= }.
 */
static int
content_auto_crop(lua_State* state)
{
	return content_method(state, "c:auto_crop", [](lua_State* state) {
		auto const context = check_content(state, 1);
		auto const& content = context.content;
		/* When the GUI offers Auto-crop (content_menu.cc) */
		if (!content->video) {
			throw LuaError("the content has no video");
		}
		if (!content->video->use()) {
			throw LuaError("the content's video is not used");
		}

		string const usage = "argument 2: expected { time = ticks }";
		if (lua_type(state, 2) != LUA_TTABLE) {
			throw LuaError(usage);
		}
		check_known_options(state, 2, { "time" });
		raw_field(state, 2, "time");
		if (lua_isnil(state, -1)) {
			throw LuaError(usage);
		}
		dcpomatic::ContentTime time;
		try {
			time = to_content_time(state, -1);
		} catch (PropertyValueError& e) {
			throw LuaError(fmt::format("option 'time': {}", e.what()));
		}
		lua_pop(state, 1);
		if (time < dcpomatic::ContentTime()) {
			throw LuaError("option 'time': expected 0 or more");
		}
		/* There is no frame to look at from the end of the video on */
		auto const length = dcpomatic::ContentTime::from_frames(content->video->length(), content->active_video_frame_rate(context.film));
		if (time >= length) {
			throw LuaError(fmt::format("option 'time': expected less than the length of the video, {} ticks", length.get()));
		}

		auto const crop = guess_crop_by_brightness(context.film, content, Config::instance()->auto_crop_threshold(), time);
		content->video->set_crop(crop);

		lua_newtable(state);
		lua_pushinteger(state, crop.left);
		lua_setfield(state, -2, "left");
		lua_pushinteger(state, crop.right);
		lua_setfield(state, -2, "right");
		lua_pushinteger(state, crop.top);
		lua_setfield(state, -2, "top");
		lua_pushinteger(state, crop.bottom);
		lua_setfield(state, -2, "bottom");
		return 1;
	});
}


/** c:copy_settings_from(other, { video = bool, audio = bool, text = bool }): take the video, audio and text settings of
 *  other, as Paste does (dcpomatic.cc); texts are paired in order, as there.
 */
static int
content_copy_settings_from(lua_State* state)
{
	return content_method(state, "c:copy_settings_from", [](lua_State* state) {
		auto const content = check_content(state, 1).content;
		auto const other = check_content(state, 2).content;
		check_known_options(state, 3, { "video", "audio", "text" });
		auto const video = check_option(state, 3, "video");
		auto const audio = check_option(state, 3, "audio");
		auto const text = check_option(state, 3, "text");
		if (!video && !audio && !text) {
			throw LuaError("argument 3: say which settings to copy, as { video = true, audio = true, text = true }");
		}

		/* Everything is checked before anything is copied */
		auto check = [](bool wanted, bool in_other, bool in_content, char const* what) {
			if (wanted && !in_other) {
				throw LuaError(fmt::format("argument 2: the content has no {}", what));
			}
			if (wanted && !in_content) {
				throw LuaError(fmt::format("the content has no {}", what));
			}
		};
		check(video, static_cast<bool>(other->video), static_cast<bool>(content->video), "video");
		check(audio, static_cast<bool>(other->audio), static_cast<bool>(content->audio), "audio");
		check(text, !other->text.empty(), !content->text.empty(), "text");
		/* Paste pairs the texts in order and ignores the extra ones; a script is told instead */
		if (text && content->text.size() != other->text.size()) {
			throw LuaError(fmt::format(
				"the content has {} texts and argument 2 has {}; texts are copied in pairs, so the numbers must be the same",
				content->text.size(), other->text.size()
				));
		}

		if (video) {
			content->video->take_settings_from(other->video);
		}
		if (audio) {
			content->audio->take_settings_from(other->audio);
		}
		if (text) {
			auto j = content->text.begin();
			auto k = other->text.begin();
			while (j != content->text.end() && k != other->text.end()) {
				(*j)->take_settings_from(*k);
				++j;
				++k;
			}
		}
		return 0;
	});
}


void
push_cpls(lua_State* state, shared_ptr<const DCPContent> dcp)
{
	/* As the content menu finds them */
	auto const cpls = dcp::find_and_resolve_cpls(dcp->directories(), true);
	lua_newtable(state);
	int n = 1;
	for (auto cpl: cpls) {
		lua_newtable(state);
		push_string(state, cpl->id());
		lua_setfield(state, -2, "id");
		push_string(state, cpl->annotation_text().get_value_or(""));
		lua_setfield(state, -2, "title");
		lua_rawseti(state, -2, n++);
	}
}


/** c:cpls(): a list of { id=, title= }, the CPLs that the content menu's Choose CPL offers */
static int
content_cpls(lua_State* state)
{
	return content_method(state, "c:cpls", [](lua_State* state) {
		push_cpls(state, dcp_part(check_content(state, 1).content));
		return 1;
	});
}


/** c:add_ov(folder): give a VF the OV it needs, as the content menu's Add OV does */
static int
content_add_ov(lua_State* state)
{
	return content_method(state, "c:add_ov", [](lua_State* state) {
		auto dcp = dcp_part(check_content(state, 1).content);
		/* When the content menu offers Add OV (content_menu.cc) */
		if (!dcp->needs_assets()) {
			throw LuaError("the DCP does not need an OV (the GUI offers Add OV only when it does)");
		}
		boost::filesystem::path const folder(check_string(state, 2));
		/* The GUI's folder dialog only gives folders that exist */
		if (!dcp::filesystem::is_directory(folder)) {
			throw LuaError(fmt::format("argument 2: {} is not a folder", folder.string()));
		}
		dcp->add_ov(folder);
		examine_content(state, dcp);
		return 0;
	});
}


/** c:add_kdm(file): decrypt the DCP with a KDM, as the content menu's Add KDM does */
static int
content_add_kdm(lua_State* state)
{
	return content_method(state, "c:add_kdm", [](lua_State* state) {
		auto dcp = dcp_part(check_content(state, 1).content);
		/* When the content menu offers Add KDM (content_menu.cc) */
		if (!dcp->encrypted()) {
			throw LuaError("the DCP is not encrypted (the GUI offers Add KDM only for one that is)");
		}
		boost::filesystem::path const file(check_string(state, 2));

		optional<dcp::EncryptedKDM> kdm;
		try {
			kdm = dcp::EncryptedKDM(dcp::file_to_string(file, MAX_KDM_SIZE));
		} catch (std::exception& e) {
			throw LuaError(fmt::format("could not load KDM: {}", e.what()));
		}

		/* As the GUI does, to give any error now */
		try {
			decrypt_kdm_with_helpful_error(*kdm);
		} catch (KDMError& e) {
			throw LuaError(fmt::format("{} {}", e.summary(), e.detail()));
		}

		auto const cpls = dcp->cpls();
		if (std::find(cpls.begin(), cpls.end(), kdm->cpl_id()) == cpls.end()) {
			throw LuaError("This KDM was not made for this DCP.  You will need a different one.");
		}
		/* The GUI's condition for this message (content_menu.cc) is never true, so its intent is used */
		if (dcp->cpl() && kdm->cpl_id() != *dcp->cpl()) {
			lua_warning(
				state,
				"This KDM was made for one of the CPLs in this DCP, but not the currently selected one.  "
				"To play the currently-selected CPL you will need a different KDM."
				);
		}

		dcp->add_kdm(*kdm);
		examine_content(state, dcp);
		return 0;
	});
}


static int
content_gc(lua_State* state)
{
	static_cast<ContentHandle*>(lua_touserdata(state, 1))->~ContentHandle();
	return 0;
}


void
register_content(lua_State* state)
{
	static luaL_Reg const methods[] = {
		{ "get", content_get },
		{ "set", content_set },
		{ "path", content_path },
		{ "index", content_index },
		{ "mapping", content_mapping },
		{ "map", content_map },
		{ "clear_mapping", content_clear_mapping },
		{ "full_length", content_full_length },
		{ "length_after_trim", content_length_after_trim },
		{ "properties", content_properties_method },
		{ "status", content_status_method },
		{ "reexamine", content_reexamine },
		{ "audio_analysis", content_audio_analysis },
		{ "subtitle_streams", content_subtitle_streams },
		{ "subtitle_colours", content_subtitle_colours },
		{ "set_subtitle_colour", content_set_subtitle_colour },
		{ "fonts", content_fonts },
		{ "set_font_file", content_set_font_file },
		{ "auto_crop", content_auto_crop },
		{ "copy_settings_from", content_copy_settings_from },
		{ "cpls", content_cpls },
		{ "add_ov", content_add_ov },
		{ "add_kdm", content_add_kdm },
		{ nullptr, nullptr }
	};
	register_metatable(state, content_metatable, methods, content_gc);
}

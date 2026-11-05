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


/** @file  src/lua/content_properties.cc
 *  @brief The tables of settings of a piece of content, and of its video, audio and text, that a
 *  script can get and set.
 *
 *  Enum-like values are the strings that metadata.xml uses (`2d', `full', `open-subtitle', `border');
 *  times are integer ticks.
 *  Optional values are nil when unset, and setting nil unsets them.
 */


#include "content_binding.h"
#include "property_table.h"
#include "lib/audio_content.h"
#include "lib/colour_conversion.h"
#include "lib/content.h"
#include "lib/dcp_content.h"
#include "lib/dcp_text_track.h"
#include "lib/ffmpeg_content.h"
#include "lib/ffmpeg_subtitle_stream.h"
#include "lib/film.h"
#include "lib/filter.h"
#include "lib/image_content.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include <dcp/language_tag.h>
#include <fmt/ranges.h>


using std::dynamic_pointer_cast;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


static vector<SkippedSetter> const kContentSkipped = {
	{ "paths", "the content's files are given to film:add_content" },
	{ "change_signals_frequent", "only tells the GUI how often to redraw while something is dragged" },
};

static vector<SkippedSetter> const kVideoSkipped = {
	{ "crop", "covered by video.left_crop, video.right_crop, video.top_crop and video.bottom_crop" },
};

static vector<SkippedSetter> const kAudioSkipped = {
	{ "mapping", "covered by c:mapping(), c:map() and c:clear_mapping()" },
	{ "stream", "takes an AudioStream, which comes from examining the content" },
};

static vector<SkippedSetter> const kTextSkipped = {};

static vector<SkippedSetter> const kFFmpegSkipped = {
	{ "default_colour_conversion", "the library calls it when the content is examined, and the GUI never does; video.colour_conversion sets the conversion" },
};

static vector<SkippedSetter> const kDCPSkipped = {
	{ "default_colour_conversion", "the library calls it when the content is examined, and the GUI never does; video.colour_conversion sets the conversion" },
};


template <class T>
static void
add_bool(PropertyTable<T>& table, string name, void (T::*set)(bool), bool (T::*get)() const)
{
	table.add(
		name,
		[set](lua_State* state, T& object, int index) { (object.*set)(to_bool(state, index)); },
		[get](lua_State* state, T const& object) { lua_pushboolean(state, (object.*get)()); }
		);
}


template <class T>
static void
add_int(PropertyTable<T>& table, string name, void (T::*set)(int), int (T::*get)() const)
{
	table.add(
		name,
		[set](lua_State* state, T& object, int index) { (object.*set)(to_int(state, index)); },
		[get](lua_State* state, T const& object) { lua_pushinteger(state, (object.*get)()); }
		);
}


template <class T>
static void
add_double(PropertyTable<T>& table, string name, void (T::*set)(double), double (T::*get)() const)
{
	table.add(
		name,
		[set](lua_State* state, T& object, int index) { (object.*set)(to_double(state, index)); },
		[get](lua_State* state, T const& object) { lua_pushnumber(state, (object.*get)()); }
		);
}


/** A string-valued property: from parses the string given to set, to makes the one get returns */
template <class T, class V>
static void
add_text(
	PropertyTable<T>& table,
	string name,
	std::function<void (T&, V)> set,
	std::function<V (T const&)> get,
	std::function<V (string const&)> from,
	std::function<string (V)> to
	)
{
	table.add(
		name,
		[set, from](lua_State* state, T& object, int index) { set(object, parse(to_string(state, index), from)); },
		[get, to](lua_State* state, T const& object) { push_string(state, to(get(object))); }
		);
}


/** As add_text, but nil unsets the value */
template <class T, class V>
static void
add_optional_text(
	PropertyTable<T>& table,
	string name,
	std::function<void (T&, optional<V>)> set,
	std::function<optional<V> (T const&)> get,
	std::function<V (string const&)> from,
	std::function<string (V)> to
	)
{
	table.add(
		name,
		[set, from](lua_State* state, T& object, int index) {
			auto const text = to_optional_string(state, index);
			set(object, text ? parse(*text, from) : optional<V>());
		},
		[get, to](lua_State* state, T const& object) {
			auto const value = get(object);
			push_optional_string(state, value ? to(*value) : optional<string>());
		}
		);
}


static dcp::LanguageTag
language_from_string(string const& text)
{
	return dcp::LanguageTag(text);
}


static string
language_to_string(dcp::LanguageTag tag)
{
	return tag.as_string();
}


/** A dcp::Colour as the AARRGGBB string that dcp::Colour reads and writes (alpha is ignored) */
static dcp::Colour
colour_from_string(string const& text)
{
	return dcp::Colour(text);
}


static string
colour_to_string(dcp::Colour colour)
{
	return colour.to_argb_string();
}


/** An optional ContentTime: integer ticks, or nil to unset it */
template <class T>
static void
add_optional_time(PropertyTable<T>& table, string name, std::function<void (T&, optional<ContentTime>)> set, std::function<optional<ContentTime> (T const&)> get)
{
	table.add(
		name,
		[set](lua_State* state, T& object, int index) {
			set(object, lua_isnil(state, index) ? optional<ContentTime>() : to_content_time(state, index));
		},
		[get](lua_State* state, T const& object) {
			auto const time = get(object);
			if (time) {
				lua_pushinteger(state, time->get());
			} else {
				lua_pushnil(state);
			}
		}
		);
}


static VideoContent&
check_video(ContentContext const& c, string const& name)
{
	if (!c.content->video) {
		throw LuaError(fmt::format("property '{}': the content has no video", name));
	}
	return *c.content->video;
}


bool
is_still(shared_ptr<const Content> content)
{
	auto image = dynamic_pointer_cast<const ImageContent>(content);
	return image && image->still();
}


/** @return the video of c; @throw LuaError naming video.length if c is not still image content */
static VideoContent&
check_still_video(ContentContext const& c)
{
	auto& video = check_video(c, "video.length");
	if (!is_still(c.content)) {
		throw LuaError("property 'video.length': only still image content has a length that can be set");
	}
	return video;
}


/** A video fade: integer ticks in Lua, frames at the content's active video frame rate in the
 *  library, converted as the GUI's video panel does.  It lives in the content table because the
 *  rate needs the film.
 */
static void
add_video_fade(PropertyTable<ContentContext>& table, string name, void (VideoContent::*set)(Frame), Frame (VideoContent::*get)() const)
{
	table.add(
		name,
		[set, name](lua_State* state, ContentContext& c, int index) {
			auto& video = check_video(c, name);
			auto const time = to_content_time(state, index);
			auto const rate = c.content->active_video_frame_rate(c.film);
			auto const frames = time.frames_round(rate);
			if (ContentTime::from_frames(frames, rate) != time) {
				throw PropertyValueError(fmt::format("expected a whole number of frames at {} fps", rate));
			}
			(video.*set)(frames);
		},
		[get, name](lua_State* state, ContentContext const& c) {
			auto const& video = check_video(c, name);
			lua_pushinteger(state, ContentTime::from_frames((video.*get)(), c.content->active_video_frame_rate(c.film)).get());
		}
		);
}


static PropertyTable<ContentContext>
make_content_properties()
{
	PropertyTable<ContentContext> table;

	table.add(
		"position",
		[](lua_State* state, ContentContext& c, int index) { c.content->set_position(c.film, to_dcp_time(state, index)); },
		[](lua_State* state, ContentContext const& c) { lua_pushinteger(state, c.content->position().get()); }
		);
	table.add(
		"trim_start",
		[](lua_State* state, ContentContext& c, int index) { c.content->set_trim_start(to_content_time(state, index)); },
		[](lua_State* state, ContentContext const& c) { lua_pushinteger(state, c.content->trim_start().get()); }
		);
	table.add(
		"trim_end",
		[](lua_State* state, ContentContext& c, int index) { c.content->set_trim_end(to_content_time(state, index)); },
		[](lua_State* state, ContentContext const& c) { lua_pushinteger(state, c.content->trim_end().get()); }
		);
	/* The frame rate to play the video at (frames per second), or nil for the content's own */
	table.add(
		"video_frame_rate",
		[](lua_State* state, ContentContext& c, int index) {
			if (lua_isnil(state, index)) {
				c.content->unset_video_frame_rate();
			} else {
				c.content->set_video_frame_rate(to_double(state, index));
			}
		},
		[](lua_State* state, ContentContext const& c) {
			auto const rate = c.content->video_frame_rate();
			if (rate) {
				lua_pushnumber(state, *rate);
			} else {
				lua_pushnil(state);
			}
		}
		);
	add_video_fade(table, "video.fade_in", &VideoContent::set_fade_in, &VideoContent::fade_in);
	add_video_fade(table, "video.fade_out", &VideoContent::set_fade_out, &VideoContent::fade_out);
	/* The length of still image content in DCP ticks, in frames at the film's rate in the library, as the
	 * timing panel's full length (timing_panel.cc); other content's length comes from examining it.
	 */
	table.add(
		"video.length",
		[](lua_State* state, ContentContext& c, int index) {
			auto& video = check_still_video(c);
			auto const length = to_dcp_time(state, index);
			if (length < DCPTime()) {
				throw PropertyValueError("expected a length of 0 or more");
			}
			video.set_length(length.frames_round(c.film->video_frame_rate()));
		},
		[](lua_State* state, ContentContext const& c) {
			auto const& video = check_still_video(c);
			lua_pushinteger(state, DCPTime::from_frames(video.length(), c.film->video_frame_rate()).get());
		}
		);
	/* Read-only: what the audio panel says about resampling (audio_panel.cc); here because it needs the film */
	table.add(
		"audio.processing_description",
		[](lua_State*, ContentContext&, int) {
			throw LuaError("property 'audio.processing_description' is read-only");
		},
		[](lua_State* state, ContentContext const& c) {
			if (!c.content->audio) {
				throw LuaError("property 'audio.processing_description': the content has no audio");
			}
			push_string(state, c.content->audio->processing_description(c.film));
		}
		);

	return table;
}


static PropertyTable<VideoContent>
make_video_properties()
{
	PropertyTable<VideoContent> table;

	add_text<VideoContent, VideoFrameType>(
		table, "video.frame_type",
		[](VideoContent& video, VideoFrameType type) { video.set_frame_type(type); },
		[](VideoContent const& video) { return video.frame_type(); },
		string_to_video_frame_type, video_frame_type_to_string
		);
	add_int(table, "video.left_crop", &VideoContent::set_left_crop, &VideoContent::requested_left_crop);
	add_int(table, "video.right_crop", &VideoContent::set_right_crop, &VideoContent::requested_right_crop);
	add_int(table, "video.top_crop", &VideoContent::set_top_crop, &VideoContent::requested_top_crop);
	add_int(table, "video.bottom_crop", &VideoContent::set_bottom_crop, &VideoContent::requested_bottom_crop);
	/* A ratio such as 1.85, or nil */
	table.add(
		"video.custom_ratio",
		[](lua_State* state, VideoContent& video, int index) {
			video.set_custom_ratio(lua_isnil(state, index) ? optional<float>() : static_cast<float>(to_double(state, index)));
		},
		[](lua_State* state, VideoContent const& video) {
			auto const ratio = video.custom_ratio();
			if (ratio) {
				lua_pushnumber(state, *ratio);
			} else {
				lua_pushnil(state);
			}
		}
		);
	/* { width = <integer>, height = <integer> }, or nil */
	table.add(
		"video.custom_size",
		[](lua_State* state, VideoContent& video, int index) {
			if (lua_isnil(state, index)) {
				video.set_custom_size({});
				return;
			}
			if (lua_type(state, index) != LUA_TTABLE) {
				throw PropertyValueError("expected { width = integer, height = integer } or nil");
			}
			raw_field(state, index, "width");
			raw_field(state, index, "height");
			try {
				video.set_custom_size(dcp::Size(to_int(state, -2), to_int(state, -1)));
			} catch (PropertyValueError&) {
				throw PropertyValueError("expected { width = integer, height = integer } or nil");
			}
			lua_pop(state, 2);
		},
		[](lua_State* state, VideoContent const& video) {
			auto const size = video.custom_size();
			if (!size) {
				lua_pushnil(state);
				return;
			}
			lua_newtable(state);
			lua_pushinteger(state, size->width);
			lua_setfield(state, -2, "width");
			lua_pushinteger(state, size->height);
			lua_setfield(state, -2, "height");
		}
		);
	/* The id of a preset (`rec709', `srgb', ...), `custom' for one that is not a preset, or nil for none */
	table.add(
		"video.colour_conversion",
		[](lua_State* state, VideoContent& video, int index) {
			auto const id = to_optional_string(state, index);
			if (id) {
				video.set_colour_conversion(parse(*id, PresetColourConversion::from_id).conversion);
			} else {
				video.unset_colour_conversion();
			}
		},
		[](lua_State* state, VideoContent const& video) {
			auto const conversion = video.colour_conversion();
			if (!conversion) {
				lua_pushnil(state);
				return;
			}
			auto const preset = conversion->preset();
			push_string(state, preset ? PresetColourConversion::all()[*preset].id : "custom");
		}
		);
	add_text<VideoContent, VideoRange>(
		table, "video.range",
		[](VideoContent& video, VideoRange range) { video.set_range(range); },
		[](VideoContent const& video) { return video.range(); },
		string_to_video_range, video_range_to_string
		);
	add_bool(table, "video.use", &VideoContent::set_use, &VideoContent::use);
	add_bool(table, "video.pq", &VideoContent::set_pq, &VideoContent::pq);
	add_optional_text<VideoContent, dcp::LanguageTag>(
		table, "video.burnt_subtitle_language",
		[](VideoContent& video, optional<dcp::LanguageTag> tag) { video.set_burnt_subtitle_language(tag); },
		[](VideoContent const& video) { return video.burnt_subtitle_language(); },
		language_from_string, language_to_string
		);

	return table;
}


static PropertyTable<AudioContent>
make_audio_properties()
{
	PropertyTable<AudioContent> table;

	/* dB */
	add_double(table, "audio.gain", &AudioContent::set_gain, &AudioContent::gain);
	/* Milliseconds; positive moves the audio later */
	add_int(table, "audio.delay", &AudioContent::set_delay, &AudioContent::delay);
	table.add(
		"audio.fade_in",
		[](lua_State* state, AudioContent& audio, int index) { audio.set_fade_in(to_content_time(state, index)); },
		[](lua_State* state, AudioContent const& audio) { lua_pushinteger(state, audio.fade_in().get()); }
		);
	table.add(
		"audio.fade_out",
		[](lua_State* state, AudioContent& audio, int index) { audio.set_fade_out(to_content_time(state, index)); },
		[](lua_State* state, AudioContent const& audio) { lua_pushinteger(state, audio.fade_out().get()); }
		);
	add_bool(table, "audio.use_same_fades_as_video", &AudioContent::set_use_same_fades_as_video, &AudioContent::use_same_fades_as_video);

	return table;
}


static PropertyTable<TextContent>
make_text_properties()
{
	PropertyTable<TextContent> table;

	add_bool(table, "text.use", &TextContent::set_use, &TextContent::use);
	add_bool(table, "text.burn", &TextContent::set_burn, &TextContent::burn);
	/* Proportions of the container's width or height */
	add_double(table, "text.x_offset", &TextContent::set_x_offset, &TextContent::x_offset);
	add_double(table, "text.y_offset", &TextContent::set_y_offset, &TextContent::y_offset);
	add_double(table, "text.x_scale", &TextContent::set_x_scale, &TextContent::x_scale);
	add_double(table, "text.y_scale", &TextContent::set_y_scale, &TextContent::y_scale);
	add_optional_text<TextContent, dcp::Colour>(
		table, "text.colour",
		[](TextContent& text, optional<dcp::Colour> colour) {
			if (colour) {
				text.set_colour(*colour);
			} else {
				text.unset_colour();
			}
		},
		[](TextContent const& text) { return text.colour(); },
		colour_from_string, colour_to_string
		);
	add_optional_text<TextContent, dcp::Effect>(
		table, "text.effect",
		[](TextContent& text, optional<dcp::Effect> effect) {
			if (effect) {
				text.set_effect(*effect);
			} else {
				text.unset_effect();
			}
		},
		[](TextContent const& text) { return text.effect(); },
		dcp::string_to_effect, dcp::effect_to_string
		);
	add_optional_text<TextContent, dcp::Colour>(
		table, "text.effect_colour",
		[](TextContent& text, optional<dcp::Colour> colour) {
			if (colour) {
				text.set_effect_colour(*colour);
			} else {
				text.unset_effect_colour();
			}
		},
		[](TextContent const& text) { return text.effect_colour(); },
		colour_from_string, colour_to_string
		);
	add_double(table, "text.line_spacing", &TextContent::set_line_spacing, &TextContent::line_spacing);
	add_optional_time<TextContent>(
		table, "text.fade_in",
		[](TextContent& text, optional<ContentTime> time) {
			if (time) {
				text.set_fade_in(*time);
			} else {
				text.unset_fade_in();
			}
		},
		[](TextContent const& text) { return text.fade_in(); }
		);
	add_optional_time<TextContent>(
		table, "text.fade_out",
		[](TextContent& text, optional<ContentTime> time) {
			if (time) {
				text.set_fade_out(*time);
			} else {
				text.unset_fade_out();
			}
		},
		[](TextContent const& text) { return text.fade_out(); }
		);
	add_int(table, "text.outline_width", &TextContent::set_outline_width, &TextContent::outline_width);
	add_text<TextContent, TextType>(
		table, "text.type",
		[](TextContent& text, TextType type) { text.set_type(type); },
		[](TextContent const& text) { return text.type(); },
		string_to_text_type, text_type_to_string
		);
	/* { name = <string>, language = <language tag or nil> }, or nil */
	table.add(
		"text.dcp_track",
		[](lua_State* state, TextContent& text, int index) {
			if (lua_isnil(state, index)) {
				text.unset_dcp_track();
				return;
			}
			string const expected = "expected { name = string, language = string or nil } or nil";
			if (lua_type(state, index) != LUA_TTABLE) {
				throw PropertyValueError(expected);
			}
			raw_field(state, index, "name");
			raw_field(state, index, "language");
			optional<string> language;
			string name;
			try {
				name = to_string(state, -2);
				language = to_optional_string(state, -1);
			} catch (PropertyValueError&) {
				throw PropertyValueError(expected);
			}
			lua_pop(state, 2);
			text.set_dcp_track(DCPTextTrack(name, language ? parse(*language, language_from_string) : optional<dcp::LanguageTag>()));
		},
		[](lua_State* state, TextContent const& text) {
			auto const track = text.dcp_track();
			if (!track) {
				lua_pushnil(state);
				return;
			}
			lua_newtable(state);
			push_string(state, track->name);
			lua_setfield(state, -2, "name");
			push_optional_string(state, track->language ? track->language->as_string() : optional<string>());
			lua_setfield(state, -2, "language");
		}
		);
	add_optional_text<TextContent, dcp::LanguageTag>(
		table, "text.language",
		[](TextContent& text, optional<dcp::LanguageTag> tag) { text.set_language(tag); },
		[](TextContent const& text) { return text.language(); },
		language_from_string, language_to_string
		);
	add_bool(table, "text.language_is_additional", &TextContent::set_language_is_additional, &TextContent::language_is_additional);

	return table;
}


static PropertyTable<FFmpegContent>
make_ffmpeg_properties()
{
	PropertyTable<FFmpegContent> table;

	/* A list of filter ids (`vflip', `yadif', ...), as the Advanced dialog sets them */
	table.add(
		"ffmpeg.filters",
		[](lua_State* state, FFmpegContent& ffmpeg, int index) {
			if (lua_type(state, index) != LUA_TTABLE) {
				throw PropertyValueError("expected a list of filter ids");
			}
			vector<Filter> filters;
			auto const size = static_cast<lua_Integer>(lua_rawlen(state, index));
			for (lua_Integer i = 1; i <= size; ++i) {
				lua_rawgeti(state, index, i);
				auto const id = lua_type(state, -1) == LUA_TSTRING ? optional<string>(lua_tostring(state, -1)) : optional<string>();
				lua_pop(state, 1);
				auto const filter = id ? Filter::from_id(*id) : optional<Filter>();
				if (!filter) {
					vector<string> ids;
					for (auto const& f: Filter::all()) {
						ids.push_back(f.id());
					}
					throw PropertyValueError(fmt::format("unknown filter '{}'; the filters are {}", id.get_value_or("?"), fmt::join(ids, ", ")));
				}
				filters.push_back(*filter);
			}
			ffmpeg.set_filters(filters);
		},
		[](lua_State* state, FFmpegContent const& ffmpeg) {
			lua_newtable(state);
			int n = 1;
			for (auto const& filter: ffmpeg.filters()) {
				push_string(state, filter.id());
				lua_rawseti(state, -2, n++);
			}
		}
		);
	/* The identifier of the subtitle stream, from c:subtitle_streams(), as the text panel chooses it; nil if there is none */
	table.add(
		"ffmpeg.subtitle_stream",
		[](lua_State* state, FFmpegContent& ffmpeg, int index) {
			auto const id = to_string(state, index);
			auto const streams = ffmpeg.subtitle_streams();
			auto stream = std::find_if(streams.begin(), streams.end(), [&id](shared_ptr<FFmpegSubtitleStream> s) { return s->identifier() == id; });
			if (stream == streams.end()) {
				if (streams.empty()) {
					throw PropertyValueError(fmt::format("no subtitle stream '{}'; the content has none", id));
				}
				vector<string> ids;
				for (auto s: streams) {
					ids.push_back(fmt::format("{} ({})", s->identifier(), s->name));
				}
				throw PropertyValueError(fmt::format("no subtitle stream '{}'; the streams are {}", id, fmt::join(ids, ", ")));
			}
			ffmpeg.set_subtitle_stream(*stream);
		},
		[](lua_State* state, FFmpegContent const& ffmpeg) {
			auto const stream = ffmpeg.subtitle_stream();
			push_optional_string(state, stream ? stream->identifier() : optional<string>());
		}
		);

	return table;
}


/** @throw LuaError, with the library's reason, unless the DCP referencing dialog would let the box for part be ticked
 *  (dcp_referencing_dialog.cc); can is the can_reference_* check of that part.
 */
static void
check_can_reference(string const& name, DCPContext const& c, string const& part, std::function<bool (string&)> can)
{
	string why_not;
	if (!c.dcp->can_reference_anything(c.film, why_not)) {
		/* The library gives no reason when it cannot read the DCP */
		throw LuaError(fmt::format("property '{}': cannot reference this DCP: {}", name, why_not.empty() ? "it could not be read (it may be missing, or its KDM may be wrong)" : why_not));
	}
	if (!can(why_not)) {
		throw LuaError(fmt::format("property '{}': cannot reference this DCP's {}: {}", name, part, why_not));
	}
}


/** @return the original type of the text that dcp.reference_text is for: the type given, or the DCP's only text's */
static TextType
reference_text_type(DCPContext const& c)
{
	string const name = "dcp.reference_text";
	TextType type;
	if (c.text_type) {
		try {
			type = parse(*c.text_type, string_to_text_type);
		} catch (PropertyValueError& e) {
			throw LuaError(fmt::format("property '{}': text type {}", name, e.what()));
		}
	} else if (c.dcp->text.size() == 1) {
		type = c.dcp->text.front()->original_type();
	} else {
		throw LuaError(fmt::format("property '{}': name the text type, 'open-subtitle' or 'closed-caption'", name));
	}
	/* The dialog's two text columns */
	if (type != TextType::OPEN_SUBTITLE && type != TextType::CLOSED_CAPTION) {
		throw LuaError(fmt::format("property '{}': the GUI references only open-subtitle and closed-caption texts", name));
	}
	return type;
}


static PropertyTable<DCPContext>
make_dcp_properties()
{
	PropertyTable<DCPContext> table;

	/* The id of the CPL to play, from c:cpls(), as the content menu's Choose CPL sets it; the DCP is examined again */
	table.add(
		"dcp.cpl",
		[](lua_State* state, DCPContext& c, int index) {
			auto const id = to_string(state, index);
			auto const cpls = c.dcp->cpls();
			if (std::find(cpls.begin(), cpls.end(), id) == cpls.end()) {
				throw PropertyValueError(fmt::format("no CPL '{}'; the CPLs are {}", id, fmt::join(cpls, ", ")));
			}
			c.dcp->set_cpl(id);
			try {
				examine_content(state, c.dcp);
			} catch (LuaError& e) {
				throw LuaError(fmt::format("property 'dcp.cpl': {}", e.what()));
			}
		},
		[](lua_State* state, DCPContext const& c) { push_optional_string(state, c.dcp->cpl()); }
		);
	/* The DCP referencing dialog's boxes: true is refused, with the library's reason, where the dialog disables the box */
	table.add(
		"dcp.reference_video",
		[](lua_State* state, DCPContext& c, int index) {
			auto const reference = to_bool(state, index);
			if (reference) {
				check_can_reference("dcp.reference_video", c, "video", [&c](string& why_not) { return c.dcp->can_reference_video(c.film, why_not); });
			}
			c.dcp->set_reference_video(reference);
		},
		[](lua_State* state, DCPContext const& c) { lua_pushboolean(state, c.dcp->reference_video()); }
		);
	table.add(
		"dcp.reference_audio",
		[](lua_State* state, DCPContext& c, int index) {
			auto const reference = to_bool(state, index);
			if (reference) {
				check_can_reference("dcp.reference_audio", c, "audio", [&c](string& why_not) { return c.dcp->can_reference_audio(c.film, why_not); });
			}
			c.dcp->set_reference_audio(reference);
		},
		[](lua_State* state, DCPContext const& c) { lua_pushboolean(state, c.dcp->reference_audio()); }
		);
	/* Takes the original type of the text, `open-subtitle' or `closed-caption', as c:set's text type */
	table.add(
		"dcp.reference_text",
		[](lua_State* state, DCPContext& c, int index) {
			auto const type = reference_text_type(c);
			auto const reference = to_bool(state, index);
			if (reference) {
				check_can_reference(
					"dcp.reference_text", c, type == TextType::OPEN_SUBTITLE ? "subtitles" : "closed captions",
					[&c, type](string& why_not) { return c.dcp->can_reference_text(c.film, type, why_not); }
					);
			}
			c.dcp->set_reference_text(type, reference);
		},
		[](lua_State* state, DCPContext const& c) { lua_pushboolean(state, c.dcp->reference_text(reference_text_type(c))); }
		);
	table.add(
		"dcp.reference_atmos",
		[](lua_State* state, DCPContext& c, int index) {
			auto const reference = to_bool(state, index);
			if (reference) {
				check_can_reference("dcp.reference_atmos", c, "Atmos", [&c](string& why_not) { return c.dcp->can_reference_atmos(c.film, why_not); });
			}
			c.dcp->set_reference_atmos(reference);
		},
		[](lua_State* state, DCPContext const& c) { lua_pushboolean(state, c.dcp->reference_atmos()); }
		);

	return table;
}


PropertyTable<ContentContext> const&
content_properties()
{
	static auto const table = make_content_properties();
	return table;
}


vector<SkippedSetter> const&
content_skipped_setters()
{
	return kContentSkipped;
}


PropertyTable<VideoContent> const&
video_properties()
{
	static auto const table = make_video_properties();
	return table;
}


vector<SkippedSetter> const&
video_skipped_setters()
{
	return kVideoSkipped;
}


PropertyTable<AudioContent> const&
audio_properties()
{
	static auto const table = make_audio_properties();
	return table;
}


vector<SkippedSetter> const&
audio_skipped_setters()
{
	return kAudioSkipped;
}


PropertyTable<TextContent> const&
text_properties()
{
	static auto const table = make_text_properties();
	return table;
}


vector<SkippedSetter> const&
text_skipped_setters()
{
	return kTextSkipped;
}


PropertyTable<FFmpegContent> const&
ffmpeg_properties()
{
	static auto const table = make_ffmpeg_properties();
	return table;
}


vector<SkippedSetter> const&
ffmpeg_skipped_setters()
{
	return kFFmpegSkipped;
}


PropertyTable<DCPContext> const&
dcp_properties()
{
	static auto const table = make_dcp_properties();
	return table;
}


vector<SkippedSetter> const&
dcp_skipped_setters()
{
	return kDCPSkipped;
}

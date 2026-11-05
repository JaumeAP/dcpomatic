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


/** @file  src/lua/film_properties.cc
 *  @brief The table of Film settings that a script can get and set.
 *
 *  Values are given as the strings that metadata.xml uses (`185' for the container, `TST' for
 *  the content type, `re-wrap' for the reuse behaviour and so on); times are integer ticks and
 *  optional values are nil when unset.
 */


#include "property_table.h"
#include "lib/audio_processor.h"
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/dcp_content_type.h"
#include "lib/film.h"
#include "lib/film_util.h"
#include "lib/ratio.h"
#include <dcp/profile.h>
#include <boost/date_time/gregorian/gregorian.hpp>


using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


static vector<SkippedSetter> const kSkipped = {
	{ "directory", "the film's directory is given to dcpomatic.open_film or dcpomatic.new_film" },
	{ "isdcf_date_today", "takes no value; set isdcf_date instead" },
	{ "marker", "takes a marker type and a time; film:set_marker() and film:unset_marker() set it" },
	{ "ui_state", "state of the GUI, written to its own file as soon as it is set" },
	{ "dirty", "private; follows from the other setters and from film:save()" },
};


/** Call f(state, -1) with each element of the array at index on top of the stack.
 *  @param expected Error message used if index is not a table or f rejects an element.
 *  A throw may leave values on the stack: Lua discards them with the error.
 */
template <class F>
static void
for_each_element(lua_State* state, int index, string const& expected, F f)
{
	if (lua_type(state, index) != LUA_TTABLE) {
		throw PropertyValueError(expected);
	}

	index = lua_absindex(state, index);
	auto const size = static_cast<lua_Integer>(lua_rawlen(state, index));
	for (lua_Integer i = 1; i <= size; ++i) {
		lua_rawgeti(state, index, i);
		try {
			f(state, -1);
		} catch (PropertyValueError&) {
			throw PropertyValueError(expected);
		}
		lua_pop(state, 1);
	}
}


static void
add_bool(PropertyTable<Film>& table, string name, void (Film::*set)(bool), bool (Film::*get)() const)
{
	table.add(
		name,
		[set](lua_State* state, Film& film, int index) { (film.*set)(to_bool(state, index)); },
		[get](lua_State* state, Film const& film) { lua_pushboolean(state, (film.*get)()); }
		);
}


static void
add_int(PropertyTable<Film>& table, string name, void (Film::*set)(int), int (Film::*get)() const)
{
	table.add(
		name,
		[set](lua_State* state, Film& film, int index) { (film.*set)(to_int(state, index)); },
		[get](lua_State* state, Film const& film) { lua_pushinteger(state, (film.*get)()); }
		);
}


static void
add_double(PropertyTable<Film>& table, string name, void (Film::*set)(double), double (Film::*get)() const)
{
	table.add(
		name,
		[set](lua_State* state, Film& film, int index) { (film.*set)(to_double(state, index)); },
		[get](lua_State* state, Film const& film) { lua_pushnumber(state, (film.*get)()); }
		);
}


static void
add_optional_string(PropertyTable<Film>& table, string name, void (Film::*set)(optional<string>), optional<string> (Film::*get)() const)
{
	table.add(
		name,
		[set](lua_State* state, Film& film, int index) { (film.*set)(to_optional_string(state, index)); },
		[get](lua_State* state, Film const& film) { push_optional_string(state, (film.*get)()); }
		);
}


/** A string-valued property: from parses the string given to set, to makes the one get returns */
template <class V>
static void
add_text(
	PropertyTable<Film>& table,
	string name,
	std::function<void (Film&, V)> set,
	std::function<V (Film const&)> get,
	std::function<V (string const&)> from,
	std::function<string (V)> to
	)
{
	table.add(
		name,
		[set, from](lua_State* state, Film& film, int index) { set(film, parse(to_string(state, index), from)); },
		[get, to](lua_State* state, Film const& film) { push_string(state, to(get(film))); }
		);
}


/** A property whose value is a language tag such as `en-US', or nil */
static void
add_optional_language(
	PropertyTable<Film>& table,
	string name,
	std::function<void (Film&, optional<dcp::LanguageTag>)> set,
	std::function<optional<dcp::LanguageTag> (Film const&)> get
	)
{
	table.add(
		name,
		[set](lua_State* state, Film& film, int index) {
			auto const text = to_optional_string(state, index);
			set(film, text ? parse(*text, [](string const& t) { return dcp::LanguageTag(t); }) : optional<dcp::LanguageTag>());
		},
		[get](lua_State* state, Film const& film) {
			auto const tag = get(film);
			push_optional_string(state, tag ? tag->as_string() : optional<string>());
		}
		);
}


/** Refuse a change to a setting that the DCP panel disables for the reason given, if any.
 *  @param same true if the new value is the current one, which is no change and so is allowed.
 */
static void
check_not_fixed(bool same, optional<string> reason)
{
	if (!same && reason) {
		throw PropertyValueError(*reason + ", so it cannot change");
	}
}


/** Refuse a standard that the DCP panel's Standard list does not offer for the film now.
 *  @param standard One of the DCP panel's standards (see film::why_standard_is_not_offered).
 */
static void
check_standard_offered(Film const& film, string const& standard)
{
	auto const reason = film::why_standard_is_not_offered(&film, standard);
	if (!reason) {
		return;
	}

	string label;
	if (standard == "smpte") {
		label = "SMPTE";
	} else if (standard == "smpte-bv20") {
		label = "SMPTE (Bv2.0 only)";
	} else if (standard == "smpte-a") {
		label = "SMPTE (A only)";
	} else if (standard == "interop") {
		label = "Interop";
	} else {
		label = "MPEG2 Interop";
	}
	throw PropertyValueError(fmt::format("the DCP panel does not offer {}: {}", label, *reason));
}


/** @return the DCP panel's name for the standard of a SMPTE profile */
static string
smpte_standard(dcp::Profile profile)
{
	switch (profile) {
	case dcp::Profile::SMPTE_A:
		return "smpte-a";
	case dcp::Profile::SMPTE_BV20:
		return "smpte-bv20";
	case dcp::Profile::SMPTE_BV21:
		return "smpte";
	}

	DCPOMATIC_ASSERT(false);
	return "";
}


/** Refuse interop and encoding, with the film's SMPTE profile, unless the DCP panel offers them as a standard.
 *  @param fix What to do first when encoding is MPEG2 and interop is false, a pair the panel never offers.
 */
static void
check_standard_offered(Film const& film, bool interop, VideoEncoding encoding, string const& fix)
{
	if (encoding == VideoEncoding::MPEG2 && !interop) {
		throw PropertyValueError("the DCP panel does not offer MPEG2 with SMPTE: " + fix);
	}
	if (interop) {
		check_standard_offered(film, encoding == VideoEncoding::MPEG2 ? "mpeg2-interop" : "interop");
	} else {
		check_standard_offered(film, smpte_standard(film.smpte_profile()));
	}
}


static PropertyTable<Film>
make_film_properties()
{
	PropertyTable<Film> table;

	table.add(
		"name",
		[](lua_State* state, Film& film, int index) { film.set_name(to_string(state, index)); },
		[](lua_State* state, Film const& film) { push_string(state, film.name()); }
		);
	table.add(
		"use_isdcf_name",
		[](lua_State* state, Film& film, int index) {
			auto const use = to_bool(state, index);
			/* The DCP panel's tick box only changes it, and turning it on also cuts the name */
			if (use != film.use_isdcf_name()) {
				film::set_use_isdcf_name(film, use);
			}
		},
		[](lua_State* state, Film const& film) { lua_pushboolean(state, film.use_isdcf_name()); }
		);
	table.add(
		"dcp_content_type",
		[](lua_State* state, Film& film, int index) {
			auto const name = to_string(state, index);
			auto const type = DCPContentType::from_isdcf_name(name);
			if (!type) {
				throw PropertyValueError(fmt::format("unknown value '{}'", name));
			}
			film.set_dcp_content_type(type);
		},
		[](lua_State* state, Film const& film) {
			push_optional_string(state, film.dcp_content_type() ? film.dcp_content_type()->isdcf_name() : optional<string>());
		}
		);
	add_text<Ratio>(
		table, "container",
		[](Film& film, Ratio ratio) {
			check_not_fixed(ratio == film.container(), film::why_picture_is_fixed(film));
			film.set_container(ratio);
		},
		[](Film const& film) { return film.container(); },
		[](string const& id) { return Ratio::from_id(id); },
		[](Ratio ratio) { return ratio.id(); }
		);
	add_text<Resolution>(
		table, "resolution",
		[](Film& film, Resolution resolution) {
			check_not_fixed(resolution == film.resolution(), film::why_picture_is_fixed(film));
			film.set_resolution(resolution);
		},
		[](Film const& film) { return film.resolution(); },
		string_to_resolution, resolution_to_string
		);
	add_bool(table, "encrypt_picture", &Film::set_encrypt_picture, &Film::encrypt_picture);
	add_bool(table, "encrypt_sound", &Film::set_encrypt_sound, &Film::encrypt_sound);
	add_bool(table, "encrypt_text", &Film::set_encrypt_text, &Film::encrypt_text);
	/* The bit rate in bit/s for the film's current video_encoding, in the range the GUI allows */
	table.add(
		"video_bit_rate",
		[](lua_State* state, Film& film, int index) {
			auto const rate = to_int64(state, index);
			check_not_fixed(rate == film.video_bit_rate(film.video_encoding()), film::why_video_bit_rate_is_fixed(film));
			int64_t const minimum = 1000000;
			auto const maximum = Config::instance()->maximum_video_bit_rate(film.video_encoding());
			/* The GUI sets whole Mbit/s */
			if (rate < minimum || rate > maximum || rate % minimum != 0) {
				throw PropertyValueError(fmt::format("expected a bit rate in bit/s from {} to {}, in whole Mbit/s", minimum, maximum));
			}
			film.set_video_bit_rate(film.video_encoding(), rate);
		},
		[](lua_State* state, Film const& film) { lua_pushinteger(state, film.video_bit_rate(film.video_encoding())); }
		);
	table.add(
		"video_frame_rate",
		[](lua_State* state, Film& film, int index) {
			auto const rate = to_int(state, index);
			check_not_fixed(rate == film.video_frame_rate(), film::why_frame_rate_is_fixed(film));
			film.set_video_frame_rate(rate);
		},
		[](lua_State* state, Film const& film) { lua_pushinteger(state, film.video_frame_rate()); }
		);
	table.add(
		"audio_channels",
		[](lua_State* state, Film& film, int index) {
			auto const channels = to_int(state, index);
			check_not_fixed(channels == film.audio_channels(), film::why_audio_is_fixed(film));
			auto const minimum = dcpomatic::film::minimum_allowed_audio_channels(film);
			if (channels % 2 != 0 || channels < minimum || channels > MAX_DCP_AUDIO_CHANNELS) {
				throw PropertyValueError(fmt::format("expected an even number from {} to {}", minimum, MAX_DCP_AUDIO_CHANNELS));
			}
			film.set_audio_channels(channels);
		},
		[](lua_State* state, Film const& film) { lua_pushinteger(state, film.audio_channels()); }
		);
	table.add(
		"three_d",
		[](lua_State* state, Film& film, int index) {
			auto const three_d = to_bool(state, index);
			check_not_fixed(three_d == film.three_d(), film::why_picture_is_fixed(film));
			film.set_three_d(three_d);
		},
		[](lua_State* state, Film const& film) { lua_pushboolean(state, film.three_d()); }
		);
	add_text<boost::gregorian::date>(
		table, "isdcf_date",
		[](Film& film, boost::gregorian::date date) { film.set_isdcf_date(date); },
		[](Film const& film) { return film.isdcf_date(); },
		/* YYYY-MM-DD */
		[](string const& text) { return boost::gregorian::from_simple_string(text); },
		[](boost::gregorian::date date) { return boost::gregorian::to_iso_extended_string(date); }
		);
	add_bool(table, "sequence", &Film::set_sequence, &Film::sequence);
	table.add(
		"interop",
		[](lua_State* state, Film& film, int index) {
			auto const interop = to_bool(state, index);
			check_standard_offered(film, interop, film.video_encoding(), "set video_encoding to jpeg2000 first");
			film.set_interop(interop);
		},
		[](lua_State* state, Film const& film) { lua_pushboolean(state, film.interop()); }
		);
	add_text<VideoEncoding>(
		table, "video_encoding",
		[](Film& film, VideoEncoding encoding) {
			check_standard_offered(film, film.interop(), encoding, "set interop to true first");
			film.set_video_encoding(encoding);
		},
		[](Film const& film) { return film.video_encoding(); },
		string_to_video_encoding, video_encoding_to_string
		);
	add_text<dcp::Profile>(
		table, "smpte_profile",
		[](Film& film, dcp::Profile profile) {
			/* The DCP panel sets a profile only by choosing its SMPTE standard */
			check_standard_offered(film, smpte_standard(profile));
			film.set_smpte_profile(profile);
		},
		[](Film const& film) { return film.smpte_profile(); },
		dcp::string_to_profile, dcp::profile_to_string
		);
	table.add(
		"audio_processor",
		[](lua_State* state, Film& film, int index) {
			auto const id = to_optional_string(state, index);
			auto const processor = id ? AudioProcessor::from_id(*id) : nullptr;
			if (id && !processor) {
				throw PropertyValueError(fmt::format("unknown value '{}'", *id));
			}
			check_not_fixed(processor == film.audio_processor(), film::why_audio_is_fixed(film));
			film.set_audio_processor(processor);
		},
		[](lua_State* state, Film const& film) {
			push_optional_string(state, film.audio_processor() ? film.audio_processor()->id() : optional<string>());
		}
		);
	add_text<ReelType>(
		table, "reel_type",
		[](Film& film, ReelType type) { film.set_reel_type(type); },
		[](Film const& film) { return film.reel_type(); },
		string_to_reel_type, reel_type_to_string
		);
	table.add(
		"reel_length",
		[](lua_State* state, Film& film, int index) { film.set_reel_length(to_int64(state, index)); },
		[](lua_State* state, Film const& film) { lua_pushinteger(state, film.reel_length()); }
		);
	table.add(
		"custom_reel_boundaries",
		[](lua_State* state, Film& film, int index) {
			vector<DCPTime> boundaries;
			for_each_element(state, index, "expected table of integer ticks", [&boundaries](lua_State* s, int i) {
				boundaries.push_back(to_dcp_time(s, i));
			});
			film.set_custom_reel_boundaries(boundaries);
		},
		[](lua_State* state, Film const& film) {
			lua_newtable(state);
			int n = 1;
			for (auto boundary: film.custom_reel_boundaries()) {
				lua_pushinteger(state, boundary.get());
				lua_rawseti(state, -2, n++);
			}
		}
		);
	add_text<Film::ReuseBehaviour>(
		table, "reuse_behaviour",
		[](Film& film, Film::ReuseBehaviour behaviour) { film.set_reuse_behaviour(behaviour); },
		[](Film const& film) { return film.reuse_behaviour(); },
		film::reuse_behaviour_from_string, film::reuse_behaviour_to_string
		);
	/* A list of { agency = <URI>, label = <rating> } */
	table.add(
		"ratings",
		[](lua_State* state, Film& film, int index) {
			vector<dcp::Rating> ratings;
			for_each_element(state, index, "expected table of { agency = string, label = string }", [&ratings](lua_State* s, int i) {
				if (lua_type(s, i) != LUA_TTABLE) {
					throw PropertyValueError("");
				}
				auto const element = lua_absindex(s, i);
				raw_field(s, element, "agency");
				raw_field(s, element, "label");
				ratings.push_back(dcp::Rating(to_string(s, -2), to_string(s, -1)));
				lua_pop(s, 2);
			});
			film.set_ratings(ratings);
		},
		[](lua_State* state, Film const& film) {
			lua_newtable(state);
			int n = 1;
			for (auto const& rating: film.ratings()) {
				lua_newtable(state);
				push_string(state, rating.agency);
				lua_setfield(state, -2, "agency");
				push_string(state, rating.label);
				lua_setfield(state, -2, "label");
				lua_rawseti(state, -2, n++);
			}
		}
		);
	table.add(
		"content_versions",
		[](lua_State* state, Film& film, int index) {
			vector<string> versions;
			for_each_element(state, index, "expected table of strings", [&versions](lua_State* s, int i) {
				versions.push_back(to_string(s, i));
			});
			film.set_content_versions(versions);
		},
		[](lua_State* state, Film const& film) {
			lua_newtable(state);
			int n = 1;
			for (auto const& version: film.content_versions()) {
				push_string(state, version);
				lua_rawseti(state, -2, n++);
			}
		}
		);
	add_text<dcp::LanguageTag>(
		table, "name_language",
		[](Film& film, dcp::LanguageTag tag) { film.set_name_language(tag); },
		[](Film const& film) { return film.name_language(); },
		[](string const& text) { return dcp::LanguageTag(text); },
		[](dcp::LanguageTag tag) { return tag.as_string(); }
		);
	add_text<TerritoryType>(
		table, "territory_type",
		[](Film& film, TerritoryType type) { film.set_territory_type(type); },
		[](Film const& film) { return film.territory_type(); },
		string_to_territory_type, territory_type_to_string
		);
	/* A region subtag such as `US', or nil */
	table.add(
		"release_territory",
		[](lua_State* state, Film& film, int index) {
			auto const text = to_optional_string(state, index);
			film.set_release_territory(
				text ? parse(*text, [](string const& t) { return dcp::LanguageTag::RegionSubtag(t); }) : optional<dcp::LanguageTag::RegionSubtag>()
				);
		},
		[](lua_State* state, Film const& film) {
			auto const territory = film.release_territory();
			push_optional_string(state, territory ? territory->subtag() : optional<string>());
		}
		);
	add_optional_language(
		table, "sign_language_video_language",
		[](Film& film, optional<dcp::LanguageTag> tag) { film.set_sign_language_video_language(tag); },
		[](Film const& film) { return film.sign_language_video_language(); }
		);
	add_int(table, "version_number", &Film::set_version_number, &Film::version_number);
	add_text<dcp::Status>(
		table, "status",
		[](Film& film, dcp::Status status) { film.set_status(status); },
		[](Film const& film) { return film.status(); },
		dcp::string_to_status, dcp::status_to_string
		);
	add_optional_string(table, "chain", &Film::set_chain, &Film::chain);
	add_optional_string(table, "facility", &Film::set_facility, &Film::facility);
	add_optional_string(table, "studio", &Film::set_studio, &Film::studio);
	add_bool(table, "temp_version", &Film::set_temp_version, &Film::temp_version);
	add_bool(table, "pre_release", &Film::set_pre_release, &Film::pre_release);
	add_bool(table, "red_band", &Film::set_red_band, &Film::red_band);
	add_bool(table, "two_d_version_of_three_d", &Film::set_two_d_version_of_three_d, &Film::two_d_version_of_three_d);
	add_optional_string(table, "distributor", &Film::set_distributor, &Film::distributor);
	/* `sdr', `dci_hdr' (ISDCF HDR1) or `dolby_vision' (ISDCF DVis); the library refuses HDR for 3D and for Interop */
	add_text<DynamicRange>(
		table, "dynamic_range",
		[](Film& film, DynamicRange range) { film.set_dynamic_range(range); },
		[](Film const& film) { return film.dynamic_range(); },
		[](string const& id) {
			if (id == "sdr") {
				return DynamicRange::SDR;
			} else if (id == "dci_hdr") {
				return DynamicRange::DCI_HDR;
			} else if (id == "dolby_vision") {
				return DynamicRange::DOLBY_VISION;
			}
			throw std::runtime_error("unknown dynamic range");
		},
		[](DynamicRange range) {
			switch (range) {
			case DynamicRange::SDR:
				return string("sdr");
			case DynamicRange::DCI_HDR:
				return string("dci_hdr");
			case DynamicRange::DOLBY_VISION:
				return string("dolby_vision");
			}
			DCPOMATIC_ASSERT(false);
		}
		);
	add_double(table, "pq_target_peak", &Film::set_pq_target_peak, &Film::pq_target_peak);
	add_double(table, "pq_sdr_white", &Film::set_pq_sdr_white, &Film::pq_sdr_white);
	add_double(table, "pq_source_peak", &Film::set_pq_source_peak, &Film::pq_source_peak);
	add_bool(table, "pq_dci_white", &Film::set_pq_dci_white, &Film::pq_dci_white);
	/* { value = <number>, unit = `candela-per-square-metre' or `foot-lambert' }, or nil */
	table.add(
		"luminance",
		[](lua_State* state, Film& film, int index) {
			if (lua_isnil(state, index)) {
				film.set_luminance();
				return;
			}
			if (lua_type(state, index) != LUA_TTABLE) {
				throw PropertyValueError("expected { value = number, unit = string } or nil");
			}
			raw_field(state, index, "value");
			raw_field(state, index, "unit");
			if (lua_type(state, -2) != LUA_TNUMBER || lua_type(state, -1) != LUA_TSTRING) {
				throw PropertyValueError("expected { value = number, unit = string } or nil");
			}
			auto const value = static_cast<float>(lua_tonumber(state, -2));
			auto const unit = parse(to_string(state, -1), dcp::Luminance::string_to_unit);
			lua_pop(state, 2);
			film.set_luminance(dcp::Luminance(value, unit));
		},
		[](lua_State* state, Film const& film) {
			auto const luminance = film.luminance();
			if (!luminance) {
				lua_pushnil(state);
				return;
			}
			lua_newtable(state);
			lua_pushnumber(state, luminance->value());
			lua_setfield(state, -2, "value");
			push_string(state, dcp::Luminance::unit_to_string(luminance->unit()));
			lua_setfield(state, -2, "unit");
		}
		);
	add_optional_string(table, "full_content_title_text", &Film::set_full_content_title_text, &Film::full_content_title_text);
	add_optional_language(
		table, "audio_language",
		[](Film& film, optional<dcp::LanguageTag> tag) { film.set_audio_language(tag); },
		[](Film const& film) { return film.audio_language(); }
		);
	table.add(
		"audio_frame_rate",
		[](lua_State* state, Film& film, int index) {
			auto const rate = to_int(state, index);
			/* The GUI offers 96kHz only when the preferences allow it */
			auto const allow_96khz = Config::instance()->allow_96khz_audio();
			if (rate != 48000 && (rate != 96000 || !allow_96khz)) {
				throw PropertyValueError(allow_96khz ? "expected 48000 or 96000" : "expected 48000 (96000 needs 96kHz audio allowed in the preferences)");
			}
			film.set_audio_frame_rate(rate);
		},
		[](lua_State* state, Film const& film) { lua_pushinteger(state, film.audio_frame_rate()); }
		);

	return table;
}


PropertyTable<Film> const&
film_properties()
{
	static auto const table = make_film_properties();
	return table;
}


vector<SkippedSetter> const&
film_skipped_setters()
{
	return kSkipped;
}

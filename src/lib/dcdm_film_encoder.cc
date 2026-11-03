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


#include "audio_buffers.h"
#include "cross.h"
#include "dcdm_film_encoder.h"
#include "dcdm_quality.h"
#include "dcp_video.h"
#include "dpx_writer.h"
#include "exceptions.h"
#include "film.h"
#include "image.h"
#include "job.h"
#include "player.h"
#include "player_text.h"
#include "player_video.h"
#include "render_text.h"
#include "string_text.h"
#include "text_type.h"
#include "util.h"
#include "wav_writer.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_writer.h>
#include <dcp/filesystem.h>
#include <dcp/interop_text_asset.h>
#include <dcp/smpte_text_asset.h>
#include <dcp/text_string.h>
#include <dcp/openjpeg_image.h>
#include <fmt/format.h>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

#include "i18n.h"


using std::make_shared;
using std::set;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using std::weak_ptr;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static
std::shared_ptr<const Film>
check_film(std::shared_ptr<const Film> film)
{
	auto const quality = dcdm_source_quality(film);
	if (!quality.ok) {
		throw std::runtime_error(quality.reason);
	}
	return film;
}


static
boost::filesystem::path
check_output(boost::filesystem::path output)
{
	if (dcp::filesystem::exists(output) && !dcp::filesystem::is_empty(output)) {
		throw std::runtime_error(fmt::format(_("The DCDM output folder {} is not empty."), output.string()));
	}
	return output;
}


static
set<dcp::Channel>
used_channels(std::shared_ptr<const Film> film)
{
	set<dcp::Channel> used;
	for (auto channel: film->mapped_audio_channels()) {
		used.insert(static_cast<dcp::Channel>(channel));
	}

	return used;
}


/** @return the channels to write and their labels: the 5.1 or 7.1 layout, then any
 *  HI, VI and sign language channels that the film uses.
 */
static
vector<std::pair<dcp::Channel, string>>
channels_of(std::shared_ptr<const Film> film)
{
	auto const used = used_channels(film);
	auto const layout = dcdm_layout(used);
	auto const layout_channels = dcdm_channels(layout);
	auto const layout_labels = dcdm_channel_labels(layout);

	vector<std::pair<dcp::Channel, string>> out;
	for (size_t i = 0; i < layout_channels.size(); ++i) {
		out.push_back({ layout_channels[i], layout_labels[i] });
	}
	for (auto const& extra: dcdm_extra_channels(used)) {
		out.push_back(extra);
	}
	return out;
}


static
AudioMapping
identity_map(std::shared_ptr<const Film> film)
{
	AudioMapping map(film->audio_channels(), film->audio_channels());
	for (int i = 0; i < film->audio_channels(); ++i) {
		map.set(i, i, 1);
	}
	return map;
}


/** @return an XYZ12LE image holding the 12-bit values of an OpenJPEG XYZ image */
static
shared_ptr<Image>
to_xyz12(shared_ptr<const dcp::OpenJPEGImage> xyz)
{
	auto const size = xyz->size();
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, size, Image::Alignment::COMPACT);
	for (int y = 0; y < size.height; ++y) {
		auto row = reinterpret_cast<uint16_t*>(image->data()[0] + y * image->stride()[0]);
		for (int x = 0; x < size.width; ++x) {
			for (int c = 0; c < 3; ++c) {
				row[x * 3 + c] = static_cast<uint16_t>(xyz->data(c)[y * size.width + x] << 4);
			}
		}
	}
	return image;
}


DCDMFilmEncoder::DCDMFilmEncoder(shared_ptr<const Film> film, weak_ptr<Job> job, boost::filesystem::path output)
	: FilmEncoder(check_film(film), job)
	, _output(check_output(output))
	, _channels(channels_of(film))
	, _reels(static_cast<int>(film->reels().size()))
	, _history(200)
	, _reel_periods(film->reels())
	/* Connect before the butler starts the player, so that no Atmos is missed */
	, _atmos_connection(_player.Atmos.connect(boost::bind(&DCDMFilmEncoder::atmos, this, _1, _2, _3)))
	, _butler(
		_film,
		_player,
		identity_map(film),
		film->audio_channels(),
		AV_PIX_FMT_RGB48LE,
		VideoRange::FULL,
		Image::Alignment::PADDED,
		false,
		false,
		Butler::Video::ENABLED,
		Butler::Audio::ENABLED
		)
{

}


void
DCDMFilmEncoder::atmos(shared_ptr<const dcp::AtmosFrame> frame, DCPTime time, AtmosMetadata metadata)
{
	boost::mutex::scoped_lock lm(_atmos_mutex);

	int reel = 0;
	while (reel + 1 < static_cast<int>(_reel_periods.size()) && _reel_periods[reel].to <= time) {
		++reel;
	}

	if (reel != _atmos_reel) {
		if (_atmos_writer) {
			_atmos_writer->finalize();
			_atmos_writer.reset();
		}
		auto const dir = reel_directory(reel);
		dcp::filesystem::create_directories(dir);
		_atmos_asset = metadata.create(dcp::Fraction(_film->video_frame_rate(), 1));
		_atmos_writer = _atmos_asset->start_write(dir / "atmos.mxf");
		_atmos_reel = reel;
	}

	/* We get one video frame's worth of Atmos at a time, as the DCP writer does */
	_atmos_writer->write(frame);
}


void
DCDMFilmEncoder::finish_atmos()
{
	boost::mutex::scoped_lock lm(_atmos_mutex);
	if (_atmos_writer) {
		_atmos_writer->finalize();
		_atmos_writer.reset();
	}
}


boost::filesystem::path
DCDMFilmEncoder::reel_directory(int reel) const
{
	if (_reels == 1) {
		return _output;
	}
	return _output / fmt::format("reel_{:02d}", reel + 1);
}


void
DCDMFilmEncoder::go()
{
	if (auto job = _job.lock()) {
		job->sub(_("Encoding"));
	}

	Waker waker(Waker::Reason::ENCODING);

	vector<string> labels;
	vector<dcp::Channel> channels_to_write;
	for (auto const& channel: _channels) {
		channels_to_write.push_back(channel.first);
		labels.push_back(channel.second);
	}
	auto const three_d = _film->three_d();
	auto const video_frame_rate = _film->video_frame_rate();
	auto const video_frame = DCPTime::from_frames(1, video_frame_rate);
	int const audio_frames = video_frame.frames_round(_film->audio_frame_rate());
	int const channels = _film->audio_channels();

	vector<float> interleaved(channels * audio_frames);
	auto deinterleaved = make_shared<AudioBuffers>(channels, audio_frames);

	auto const reels = _film->reels();
	int reel_index = 0;
	vector<unique_ptr<WAVWriter>> wavs;

	auto open_reel = [&](int index) {
		auto const dir = reel_directory(index);
		dcp::filesystem::create_directories(dir / "audio");
		if (three_d) {
			dcp::filesystem::create_directories(dir / "left");
			dcp::filesystem::create_directories(dir / "right");
		}
		wavs.clear();
		for (auto const& label: labels) {
			wavs.push_back(unique_ptr<WAVWriter>(new WAVWriter(dir / "audio" / (label + ".wav"), _film->audio_frame_rate())));
		}
	};

	open_reel(0);

	for (DCPTime time; time < _film->length(); time += video_frame) {

		while (reel_index + 1 < static_cast<int>(reels.size()) && !reels[reel_index].contains(time)) {
			for (auto& wav: wavs) {
				wav->close();
			}
			++reel_index;
			open_reel(reel_index);
		}

		auto const reel = reels[reel_index];
		auto const dir = reel_directory(reel_index);

		for (int i = 0; i < (three_d ? 2 : 1); ++i) {
			Butler::Error error;
			auto video = _butler.get_video(Butler::Behaviour::BLOCKING, &error);
			_butler.rethrow();
			if (!video.first) {
				if (error.code != Butler::Error::Code::FINISHED) {
					throw DecodeError(fmt::format("Error during decoding: {}", error.summary()));
				}
				continue;
			}

			auto const index = (video.second - reel.from).frames_round(video_frame_rate);
			auto const name = fmt::format("frame_{:08d}.dpx", index);
			auto path = dir / name;
			if (three_d) {
				path = dir / (video.first->eyes() == Eyes::RIGHT ? "right" : "left") / name;
			}

			write_dpx(path, to_xyz12(DCPVideo::convert_to_xyz(video.first)));
		}

		_butler.get_audio(Butler::Behaviour::BLOCKING, interleaved.data(), audio_frames);
		float const* p = interleaved.data();
		for (int j = 0; j < audio_frames; ++j) {
			for (int k = 0; k < channels; ++k) {
				deinterleaved->data(k)[j] = *p++;
			}
		}
		auto const layout = dcdm_pick_channels(deinterleaved, channels_to_write);
		for (size_t c = 0; c < wavs.size(); ++c) {
			wavs[c]->write(layout->data(static_cast<int>(c)), audio_frames);
		}

		_history.event();
		{
			boost::mutex::scoped_lock lm(_mutex);
			_last_time = time;
		}

		if (auto job = _job.lock()) {
			job->set_progress(float(time.get()) / _film->length().get());
		}

		waker.nudge();
	}

	for (auto& wav: wavs) {
		wav->close();
	}

	/* Stop the player sending more Atmos and close the last asset */
	_atmos_connection.disconnect();
	finish_atmos();

	write_text();
}


/** Correct a subtitle's vertical position for the standard we are writing, as ReelWriter does */
static
float
vertical_position(StringText const& subtitle, dcp::SubtitleStandard to, FontMetrics& metrics)
{
	if (dcp::uses_baseline(subtitle.valign_standard) == dcp::uses_baseline(to)) {
		return subtitle.v_position();
	}

	auto const baseline_to_bottom = metrics.baseline_to_bottom(subtitle);
	auto const height = metrics.height(subtitle);

	float correction = 0;
	switch (subtitle.v_align()) {
	case dcp::VAlign::TOP:
		correction = height - baseline_to_bottom;
		break;
	case dcp::VAlign::CENTER:
		correction = (height / 2) - baseline_to_bottom;
		break;
	case dcp::VAlign::BOTTOM:
		correction = baseline_to_bottom;
		break;
	}

	return subtitle.v_position() + (dcp::uses_bounding_box(subtitle.valign_standard) ? correction : -correction);
}


static
string
safe_name(string s)
{
	for (auto& c: s) {
		if (!isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
			c = '_';
		}
	}
	return s;
}


void
DCDMFilmEncoder::write_text()
{
	/* Key is (reel, track); no track means open subtitles */
	std::map<std::pair<int, boost::optional<DCPTextTrack>>, shared_ptr<dcp::TextAsset>> assets;
	vector<DCPTextTrack> tracks;

	auto const interop = _film->interop();
	auto const standard = interop ? dcp::SubtitleStandard::INTEROP : dcp::SubtitleStandard::SMPTE_2014;
	FontMetrics metrics(_film->frame_size().height);
	dcp::ArrayData const default_font(default_font_file());
	auto const languages = _film->open_text_languages();

	auto make_asset = [&](int reel, boost::optional<DCPTextTrack> track) -> shared_ptr<dcp::TextAsset> {
		if (interop) {
			auto asset = make_shared<dcp::InteropTextAsset>();
			asset->set_movie_title(_film->name());
			if (!track) {
				asset->set_language(languages.first ? languages.first->as_string() : "Unknown");
			} else if (track->language) {
				asset->set_language(track->language->as_string());
			}
			asset->set_reel_number(fmt::to_string(reel + 1));
			return asset;
		}

		auto asset = make_shared<dcp::SMPTETextAsset>();
		asset->set_content_title_text(_film->name());
		if (!track && languages.first) {
			asset->set_language(*languages.first);
		} else if (track && track->language) {
			asset->set_language(dcp::LanguageTag(track->language->as_string()));
		}
		asset->set_edit_rate(dcp::Fraction(_film->video_frame_rate(), 1));
		asset->set_reel_number(reel + 1);
		asset->set_time_code_rate(_film->video_frame_rate());
		asset->set_start_time(dcp::Time());
		return asset;
	};

	Player player(_film, Image::Alignment::PADDED, false);
	player.set_ignore_video();
	player.set_ignore_audio();

	boost::signals2::scoped_connection connection = player.Text.connect(
		[&](PlayerText subs, TextType type, boost::optional<DCPTextTrack> track, DCPTimePeriod period) {
			if (subs.string.empty()) {
				return;
			}

			boost::optional<DCPTextTrack> key_track;
			if (type == TextType::CLOSED_CAPTION || type == TextType::CLOSED_SUBTITLE) {
				DCPOMATIC_ASSERT(track);
				key_track = track;
				if (std::find(tracks.begin(), tracks.end(), *track) == tracks.end()) {
					tracks.push_back(*track);
				}
			} else if (type != TextType::OPEN_SUBTITLE && type != TextType::OPEN_CAPTION) {
				return;
			}

			int reel = 0;
			while (reel + 1 < static_cast<int>(_reel_periods.size()) && _reel_periods[reel].to <= period.from) {
				++reel;
			}
			auto const reel_period = _reel_periods[reel];

			auto& asset = assets[std::make_pair(reel, key_track)];
			if (!asset) {
				asset = make_asset(reel, key_track);
			}

			/* As ReelWriter: times relative to the reel, to the millisecond */
			int const tcr = 1000;
			for (auto i: subs.string) {
				i.set_in(dcp::Time(period.from.seconds() - reel_period.from.seconds(), tcr));
				i.set_out(dcp::Time(period.to.seconds() - reel_period.from.seconds(), tcr));
				i.set_v_position(vertical_position(i, standard, metrics));
				auto text = make_shared<dcp::TextString>(i);
				auto const id = i.font ? i.font->id() : std::string("default");
				text->set_font(id);
				asset->add(text);
				asset->ensure_font(id, i.font && i.font->data() ? *i.font->data() : default_font);
			}
		});

	while (!player.pass()) {}

	for (auto const& i: assets) {
		auto const reel = i.first.first;
		auto const& track = i.first.second;
		boost::filesystem::path dir = reel_directory(reel);
		boost::filesystem::path name;
		if (track) {
			auto const index = std::find(tracks.begin(), tracks.end(), *track) - tracks.begin();
			dir /= "captions";
			name = fmt::format("{:02d}_{}.xml", index + 1, safe_name(track->name));
		} else {
			dir /= "subtitles";
			name = "subtitles.xml";
		}

		dcp::filesystem::create_directories(dir);
		if (interop) {
			/* This writes the fonts alongside the XML */
			i.second->write(dir / name);
		} else {
			std::ofstream xml((dir / name).string());
			xml << i.second->xml_as_string();
			for (auto const& font: i.second->font_data()) {
				font.second.write(dir / (safe_name(font.first) + ".ttf"));
			}
		}
	}
}


boost::optional<float>
DCDMFilmEncoder::current_rate() const
{
	return _history.rate();
}


Frame
DCDMFilmEncoder::frames_done() const
{
	boost::mutex::scoped_lock lm(_mutex);
	return _last_time.frames_round(_film->video_frame_rate());
}

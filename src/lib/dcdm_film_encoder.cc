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
#include "player_video.h"
#include "wav_writer.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_writer.h>
#include <dcp/filesystem.h>
#include <dcp/openjpeg_image.h>
#include <fmt/format.h>
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
DCDMLayout
layout_of(std::shared_ptr<const Film> film)
{
	set<dcp::Channel> used;
	for (auto channel: film->mapped_audio_channels()) {
		used.insert(static_cast<dcp::Channel>(channel));
	}
	return dcdm_layout(used);
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
	, _layout(layout_of(film))
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

	auto const labels = dcdm_channel_labels(_layout);
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
		auto const layout = dcdm_layout_channels(deinterleaved, _layout);
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

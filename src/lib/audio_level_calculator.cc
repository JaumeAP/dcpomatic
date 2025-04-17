/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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
#include "audio_level_calculator.h"


using std::make_pair;
using std::shared_ptr;
using boost::optional;


AudioLevelCalculator::AudioLevelCalculator(int calculation_frame_rate, int falloff)
	: _calculation_frame_rate(calculation_frame_rate)
	, _falloff_linear(pow(10, -falloff / (calculation_frame_rate * 20.0f)))
{

}


void
AudioLevelCalculator::put(shared_ptr<const AudioBuffers> audio, dcpomatic::DCPTime time, int frame_rate)
{
	int const frames_per_measurement = frame_rate / _calculation_frame_rate;

	boost::mutex::scoped_lock lm(_current_mutex);

	auto const channels = audio->channels();

	if (static_cast<int>(_current_peaks.size()) != channels) {
		_current_peaks.resize(channels);
	}

	auto const data = audio->data();
	auto const frames = audio->frames();
	for (auto frame = 0; frame < frames; ++frame) {
		for (auto channel = 0; channel < channels; ++channel) {
			_current_peaks[channel] = std::max(std::abs(data[channel][frame]), _current_peaks[channel]);
		}

		++_current_frames;
		if (_current_frames == frames_per_measurement) {
			{
				boost::mutex::scoped_lock lm(_store_mutex);
				_levels.emplace_back(time + dcpomatic::DCPTime::from_frames(frame + 1, frame_rate), _current_peaks);
			}
			for (auto channel = 0; channel < channels; ++channel) {
				_current_peaks[channel] *= _falloff_linear;
			}
			_current_frames = 0;
		}
	}

}


void
AudioLevelCalculator::clear()
{
	boost::mutex::scoped_lock slm(_store_mutex);
	_levels.clear();

	boost::mutex::scoped_lock clm(_current_mutex);
	std::fill(_current_peaks.begin(), _current_peaks.end(), 0.0f);
	_current_frames = 0;
}


boost::optional<AudioLevels>
AudioLevelCalculator::get()
{
	boost::mutex::scoped_lock lm(_store_mutex);
	if (_levels.empty()) {
		return {};
	}
	auto const m = _levels.front();
	_levels.pop_front();
	return m;
}


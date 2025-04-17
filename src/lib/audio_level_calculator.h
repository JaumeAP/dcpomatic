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


#ifndef DCPOMATIC_LEVEL_CALCULATOR_H
#define DCPOMATIC_LEVEL_CALCULATOR_H


#include "audio_levels.h"
#include "dcpomatic_time.h"
#include <boost/thread/mutex.hpp>
#include <memory>


class AudioBuffers;


class AudioLevelCalculator
{
public:
	/** @param calculation_frame_rate number of peak calculations to make per second
	 *  @param falloff meter falloff in db/sec
	 */
	AudioLevelCalculator(int calculation_frame_rate = 30, int falloff = 15);

	AudioLevelCalculator(AudioLevelCalculator const&) = delete;
	AudioLevelCalculator& operator=(AudioLevelCalculator const&) = delete;

	void put(std::shared_ptr<const AudioBuffers> audio, dcpomatic::DCPTime time, int frame_rate);
	void clear();
	boost::optional<AudioLevels> get();

	dcpomatic::DCPTime time_between_measurements() const {
		return dcpomatic::DCPTime::from_seconds(1) / _calculation_frame_rate;
	};

private:
	int const _calculation_frame_rate;
	float const _falloff_linear;

	boost::mutex _current_mutex;
	std::vector<float> _current_peaks;
	int _current_frames = 0;

	mutable boost::mutex _store_mutex;
	std::list<AudioLevels> _levels;
};


#endif


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


#include "dcpomatic_time.h"
#include <boost/thread.hpp>
#include <memory>


class AudioBuffers;


class LevelCalculator
{
public:
	LevelCalculator();

	LevelCalculator(LevelCalculator const&) = delete;
	LevelCalculator& operator=(LevelCalculator const&) = delete;

	void put(std::shared_ptr<const AudioBuffers> audio, dcpomatic::DCPTime time, int frame_rate);
	void clear();
	std::vector<float> get(dcpomatic::DCPTime time);

	void enable(bool e);

private:
	boost::mutex _current_mutex;
	std::vector<float> _current_peaks;
	int _current_frames = 0;

	struct Measurement
	{
		Measurement(dcpomatic::DCPTime t, std::vector<float> v)
			: time(t)
			, value(std::move(v))
		{

		}

		dcpomatic::DCPTime time;
		std::vector<float> value;
	};

	mutable boost::mutex _store_mutex;
	std::list<Measurement> _peaks;

	boost::atomic<bool> _enabled;
};


#endif


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


#include "lib/audio_buffers.h"
#include "lib/level_calculator.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(level_calculator_falloff_correct)
{
	auto pulse = make_shared<AudioBuffers>(6, 96000);
	pulse->make_silent();

	pulse->data()[0][0] = 1;

	LevelCalculator calc(30, 15);
	calc.enable(true);
	calc.put(pulse, {}, 48000);

	BOOST_CHECK_CLOSE(calc.get(dcpomatic::DCPTime::from_frames(31, 30))[0], pow(10, -15.0 / 20), 0.0001);
}


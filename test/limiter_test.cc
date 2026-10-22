/*
    Copyright (C) 2026 DCP-o-matic contributors

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
#include "lib/util.h"
#include <boost/test/unit_test.hpp>
#include <cmath>


using std::make_shared;


BOOST_AUTO_TEST_CASE(limiter_test)
{
	/* Quiet audio is left alone */
	auto quiet = make_shared<AudioBuffers>(2, 100);
	for (int i = 0; i < 100; ++i) {
		quiet->data(0)[i] = 0.5f;
		quiet->data(1)[i] = -0.25f;
	}
	float gain = 1;
	limit_peaks(quiet, gain, 48000);
	BOOST_CHECK_EQUAL(gain, 1.0f);
	BOOST_CHECK_EQUAL(quiet->data(0)[99], 0.5f);
	BOOST_CHECK_EQUAL(quiet->data(1)[99], -0.25f);

	/* A peak of 3 on one channel is brought to 1 on that channel, and the other channel follows */
	auto loud = make_shared<AudioBuffers>(2, 48000);
	loud->make_silent();
	for (int i = 0; i < 48000; ++i) {
		loud->data(1)[i] = 0.5f;
	}
	loud->data(0)[10] = 3.0f;
	gain = 1;
	limit_peaks(loud, gain, 48000);
	BOOST_CHECK_CLOSE(loud->data(0)[10], 1.0f, 0.01);
	BOOST_CHECK_CLOSE(loud->data(1)[10], 0.5f / 3, 0.01);
	for (int i = 0; i < 48000; ++i) {
		BOOST_REQUIRE(fabsf(loud->data(0)[i]) <= 1.0f);
	}

	/* The gain comes back to 1 within a second */
	BOOST_CHECK(gain > 0.99f);
}


/** The gain carries over from one buffer to the next: after a peak at the end of one buffer the next starts quieter and recovers */
BOOST_AUTO_TEST_CASE(limiter_gain_carries_over_test)
{
	auto first = make_shared<AudioBuffers>(1, 100);
	first->make_silent();
	first->data(0)[99] = 2.0f;
	float gain = 1;
	limit_peaks(first, gain, 48000);
	BOOST_CHECK_CLOSE(gain, 0.5f, 0.01);

	auto second = make_shared<AudioBuffers>(1, 100);
	for (int i = 0; i < 100; ++i) {
		second->data(0)[i] = 0.5f;
	}
	limit_peaks(second, gain, 48000);
	BOOST_CHECK_CLOSE(second->data(0)[0], 0.25f, 0.1);
	BOOST_CHECK(second->data(0)[99] > second->data(0)[0]);
}

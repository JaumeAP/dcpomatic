/*
    Copyright (C) 2026 Jaume Puig

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


/** @file  test/downmix_test.cc
 *  @brief Test the 5.1 to stereo, 7.1 to stereo and 7.1 to 5.1 downmix maps (doc/design/downmix_7_1_to_5_1.md).
 *  @ingroup selfcontained
 */


#include "lib/audio_mapping.h"
#include "lib/config.h"
#include "lib/constants.h"
#include <dcp/types.h>
#include <boost/test/unit_test.hpp>
#include <cmath>


static int
ch(dcp::Channel channel)
{
	return static_cast<int>(channel);
}


static void
check_gain(AudioMapping const& map, dcp::Channel input, int output, float expected)
{
	BOOST_TEST_INFO("input " << ch(input) << " output " << output);
	if (expected == 0) {
		BOOST_CHECK_SMALL(map.get(ch(input), output), 1e-6F);
	} else {
		BOOST_CHECK_CLOSE(map.get(ch(input), output), expected, 0.01);
	}
}


/** 5.1 to stereo: the rule the stereo mixdown export has always used, L = g * (L + 0.707 C + Ls) with g = 2 / (4 + sqrt(2)) */
BOOST_AUTO_TEST_CASE(downmix_5_1_to_stereo_test)
{
	float const g = 2 / (4 + std::sqrt(2.0F));
	float const c = g / std::sqrt(2.0F);

	auto const map = stereo_downmix_map(6, MAX_DCP_AUDIO_CHANNELS);

	check_gain(map, dcp::Channel::LEFT, 0, g);
	check_gain(map, dcp::Channel::LEFT, 1, 0);
	check_gain(map, dcp::Channel::RIGHT, 0, 0);
	check_gain(map, dcp::Channel::RIGHT, 1, g);
	check_gain(map, dcp::Channel::CENTRE, 0, c);
	check_gain(map, dcp::Channel::CENTRE, 1, c);
	check_gain(map, dcp::Channel::LS, 0, g);
	check_gain(map, dcp::Channel::LS, 1, 0);
	check_gain(map, dcp::Channel::RS, 0, 0);
	check_gain(map, dcp::Channel::RS, 1, g);
	check_gain(map, dcp::Channel::LFE, 0, 0);
	check_gain(map, dcp::Channel::LFE, 1, 0);
	check_gain(map, dcp::Channel::BSL, 0, 0);
	check_gain(map, dcp::Channel::BSR, 1, 0);

	/* Full-level coherent content cannot overflow: the gains into each output add up to 1 */
	BOOST_CHECK_CLOSE(map.get(ch(dcp::Channel::LEFT), 0) + map.get(ch(dcp::Channel::CENTRE), 0) + map.get(ch(dcp::Channel::LS), 0), 1.0F, 0.01);
	BOOST_CHECK_CLOSE(map.get(ch(dcp::Channel::RIGHT), 1) + map.get(ch(dcp::Channel::CENTRE), 1) + map.get(ch(dcp::Channel::RS), 1), 1.0F, 0.01);
}


/** 7.1 goes through 5.1 first: L, R and C keep the 5.1 gains and each of the four surrounds enters at -3 dB of the 5.1 surround gain */
BOOST_AUTO_TEST_CASE(downmix_7_1_to_stereo_test)
{
	float const g = 2 / (4 + std::sqrt(2.0F));
	float const c = g / std::sqrt(2.0F);

	auto const map = stereo_downmix_map(MAX_DCP_AUDIO_CHANNELS, MAX_DCP_AUDIO_CHANNELS);

	check_gain(map, dcp::Channel::LEFT, 0, g);
	check_gain(map, dcp::Channel::RIGHT, 1, g);
	check_gain(map, dcp::Channel::CENTRE, 0, c);
	check_gain(map, dcp::Channel::CENTRE, 1, c);
	check_gain(map, dcp::Channel::LS, 0, c);
	check_gain(map, dcp::Channel::LS, 1, 0);
	check_gain(map, dcp::Channel::BSL, 0, c);
	check_gain(map, dcp::Channel::BSL, 1, 0);
	check_gain(map, dcp::Channel::RS, 1, c);
	check_gain(map, dcp::Channel::RS, 0, 0);
	check_gain(map, dcp::Channel::BSR, 1, c);
	check_gain(map, dcp::Channel::BSR, 0, 0);
	check_gain(map, dcp::Channel::LFE, 0, 0);
	check_gain(map, dcp::Channel::LFE, 1, 0);

	/* A 7.1 film is not louder than a 5.1 one in L, R and C */
	auto const five_one = stereo_downmix_map(6, MAX_DCP_AUDIO_CHANNELS);
	for (auto channel: { dcp::Channel::LEFT, dcp::Channel::RIGHT, dcp::Channel::CENTRE }) {
		for (int output = 0; output < 2; ++output) {
			BOOST_TEST_INFO("input " << ch(channel) << " output " << output);
			BOOST_CHECK_EQUAL(map.get(ch(channel), output), five_one.get(ch(channel), output));
		}
	}
}


/** 2-channel films are passed through unchanged */
BOOST_AUTO_TEST_CASE(downmix_stereo_film_test)
{
	auto const map = stereo_downmix_map(2, MAX_DCP_AUDIO_CHANNELS);
	check_gain(map, dcp::Channel::LEFT, 0, 1);
	check_gain(map, dcp::Channel::LEFT, 1, 0);
	check_gain(map, dcp::Channel::RIGHT, 0, 0);
	check_gain(map, dcp::Channel::RIGHT, 1, 1);
	check_gain(map, dcp::Channel::CENTRE, 0, 0);
}


/** Monitor with 6 outputs and a 7.1 film: Ls = 0.707 Lss + 0.707 Lrs, Rs = 0.707 Rss + 0.707 Rrs, everything else 1:1 */
BOOST_AUTO_TEST_CASE(downmix_7_1_to_5_1_monitor_test)
{
	float const minus_3dB = 1 / std::sqrt(2.0F);

	auto const map = Config::instance()->audio_mapping(6, MAX_DCP_AUDIO_CHANNELS);
	BOOST_CHECK_EQUAL(map.input_channels(), MAX_DCP_AUDIO_CHANNELS);
	BOOST_CHECK_EQUAL(map.output_channels(), 6);

	for (auto channel: { dcp::Channel::LEFT, dcp::Channel::RIGHT, dcp::Channel::CENTRE, dcp::Channel::LFE }) {
		check_gain(map, channel, ch(channel), 1);
	}
	check_gain(map, dcp::Channel::LS, ch(dcp::Channel::LS), minus_3dB);
	check_gain(map, dcp::Channel::RS, ch(dcp::Channel::RS), minus_3dB);
	check_gain(map, dcp::Channel::BSL, ch(dcp::Channel::LS), minus_3dB);
	check_gain(map, dcp::Channel::BSR, ch(dcp::Channel::RS), minus_3dB);

	/* No cross-feed between sides */
	check_gain(map, dcp::Channel::LS, ch(dcp::Channel::RS), 0);
	check_gain(map, dcp::Channel::RS, ch(dcp::Channel::LS), 0);
	check_gain(map, dcp::Channel::BSL, ch(dcp::Channel::RS), 0);
	check_gain(map, dcp::Channel::BSR, ch(dcp::Channel::LS), 0);
}


/** Monitor with 6 outputs and a 5.1 film passes unchanged: the rear channels are not folded */
BOOST_AUTO_TEST_CASE(downmix_5_1_monitor_passthrough_test)
{
	auto const map = Config::instance()->audio_mapping(6, 6);
	BOOST_CHECK_EQUAL(map.output_channels(), 6);

	for (int i = 0; i < 6; ++i) {
		BOOST_TEST_INFO("channel " << i);
		BOOST_CHECK_CLOSE(map.get(i, i), 1.0F, 0.01);
	}
	check_gain(map, dcp::Channel::BSL, ch(dcp::Channel::LS), 0);
	check_gain(map, dcp::Channel::BSR, ch(dcp::Channel::RS), 0);

	/* Leave the cached default for a full-size film, as the Player would regenerate it */
	Config::instance()->audio_mapping(6, MAX_DCP_AUDIO_CHANNELS);
}

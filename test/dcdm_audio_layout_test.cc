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


#include "lib/audio_buffers.h"
#include "lib/dcdm_audio_layout.h"
#include <boost/test/unit_test.hpp>
#include <memory>
#include <set>


using std::make_shared;
using std::set;


BOOST_AUTO_TEST_CASE(dcdm_layout_51_test)
{
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::RIGHT }) == DCDMLayout::FIVE_ONE);
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::RIGHT, dcp::Channel::CENTRE, dcp::Channel::LFE, dcp::Channel::LS, dcp::Channel::RS }) == DCDMLayout::FIVE_ONE);
	BOOST_CHECK(dcdm_layout({}) == DCDMLayout::FIVE_ONE);
}


BOOST_AUTO_TEST_CASE(dcdm_layout_71_test)
{
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::BSL }) == DCDMLayout::SEVEN_ONE);
	BOOST_CHECK(dcdm_layout({ dcp::Channel::BSR }) == DCDMLayout::SEVEN_ONE);
}


BOOST_AUTO_TEST_CASE(dcdm_layout_rejects_hi_vi_test)
{
	try {
		dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::HI, dcp::Channel::VI });
		BOOST_CHECK_MESSAGE(false, "no exception thrown");
	} catch (std::runtime_error& e) {
		BOOST_CHECK(std::string(e.what()).find("HI") != std::string::npos);
		BOOST_CHECK(std::string(e.what()).find("VI") != std::string::npos);
	}
	BOOST_CHECK_THROW(dcdm_layout({ dcp::Channel::LC }), std::runtime_error);
	BOOST_CHECK_THROW(dcdm_layout({ dcp::Channel::RC }), std::runtime_error);
	/* Non-programme channels are ignored */
	BOOST_CHECK(dcdm_layout({ dcp::Channel::LEFT, dcp::Channel::SYNC_SIGNAL }) == DCDMLayout::FIVE_ONE);
}


BOOST_AUTO_TEST_CASE(dcdm_labels_test)
{
	auto const five = dcdm_channel_labels(DCDMLayout::FIVE_ONE);
	BOOST_REQUIRE_EQUAL(five.size(), 6U);
	BOOST_CHECK_EQUAL(five[0], "L");
	BOOST_CHECK_EQUAL(five[1], "R");
	BOOST_CHECK_EQUAL(five[2], "C");
	BOOST_CHECK_EQUAL(five[3], "LFE");
	BOOST_CHECK_EQUAL(five[4], "Ls");
	BOOST_CHECK_EQUAL(five[5], "Rs");

	auto const seven = dcdm_channel_labels(DCDMLayout::SEVEN_ONE);
	BOOST_REQUIRE_EQUAL(seven.size(), 8U);
	BOOST_CHECK_EQUAL(seven[6], "Lrs");
	BOOST_CHECK_EQUAL(seven[7], "Rrs");
}


BOOST_AUTO_TEST_CASE(dcdm_layout_channels_stereo_test)
{
	auto in = make_shared<AudioBuffers>(2, 4);
	for (int i = 0; i < 4; ++i) {
		in->data(0)[i] = 0.25f;
		in->data(1)[i] = -0.5f;
	}
	auto out = dcdm_layout_channels(in, DCDMLayout::FIVE_ONE);
	BOOST_REQUIRE_EQUAL(out->channels(), 6);
	BOOST_REQUIRE_EQUAL(out->frames(), 4);
	for (int i = 0; i < 4; ++i) {
		BOOST_CHECK_EQUAL(out->data(0)[i], 0.25f);
		BOOST_CHECK_EQUAL(out->data(1)[i], -0.5f);
		for (int c = 2; c < 6; ++c) {
			BOOST_CHECK_EQUAL(out->data(c)[i], 0.0f);
		}
	}
}


BOOST_AUTO_TEST_CASE(dcdm_layout_channels_71_test)
{
	/* 12 DCP channels: value is channel index / 100 */
	auto in = make_shared<AudioBuffers>(12, 2);
	for (int c = 0; c < 12; ++c) {
		for (int i = 0; i < 2; ++i) {
			in->data(c)[i] = c / 100.0f;
		}
	}
	auto out = dcdm_layout_channels(in, DCDMLayout::SEVEN_ONE);
	BOOST_REQUIRE_EQUAL(out->channels(), 8);
	int const expected_source[] = { 0, 1, 2, 3, 4, 5, 10, 11 };
	for (int c = 0; c < 8; ++c) {
		BOOST_CHECK_EQUAL(out->data(c)[0], expected_source[c] / 100.0f);
	}
}

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


#include "lib/atmos_mxf_content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/reel_atmos_asset.h>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::vector;


BOOST_AUTO_TEST_CASE(reuse_whole_dcp_test)
{
	boost::filesystem::path const source = "test/data/scaling_test_133_185";
	auto dcp = make_shared<DCPContent>(source);
	auto film = new_test_film("reuse_basic_test", { dcp });
	film->set_container(Ratio::from_id("185"));
	film->set_audio_channels(8);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
		});

	auto const output = film->dir(film->dcp_name());
	check_file(source / "j2c_97b50c93-ad3a-45d7-8ce0-4b875302a704.mxf", output / "j2c_97b50c93-ad3a-45d7-8ce0-4b875302a704.mxf");
	check_file(source / "pcm_156782ba-4cae-4468-b3ea-1126be23b6d6.mxf", output / "pcm_156782ba-4cae-4468-b3ea-1126be23b6d6.mxf");
}


BOOST_AUTO_TEST_CASE(reuse_atmos_mxf_test)
{
	auto atmos = make_shared<AtmosMXFContent>("test/data/atmos_0.mxf");
	auto film = new_test_film("reuse_atmos_mxf_test", { atmos });
	film->set_interop(false);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
		});

	auto const output = film->dir(film->dcp_name());

	check_file("test/data/atmos_0.mxf", output / "atmos_0.mxf");

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 1U);
	auto reel = dcp.cpls()[0]->reels()[0];
	BOOST_REQUIRE(reel->atmos());
	BOOST_CHECK_EQUAL(reel->atmos()->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(reel->atmos()->actual_duration(), 240);
}


BOOST_AUTO_TEST_CASE(reuse_atmos_for_one_reel_mxf_test)
{
	auto red = content_factory("test/data/flat_red.png")[0];
	auto atmos = make_shared<AtmosMXFContent>("test/data/atmos_0.mxf");
	auto film = new_test_film("reuse_atmos_for_one_reel_mxf_test", { atmos, red });
	film->set_interop(false);

	/* 30s of red with 10s of ATMOS in the middle */
	red->video->set_length(240 * 3);
	atmos->set_position(film, dcpomatic::DCPTime::from_seconds(10));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({
		dcpomatic::DCPTime::from_seconds(10),
		dcpomatic::DCPTime::from_seconds(20)
	});

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
		});

	auto const output = film->dir(film->dcp_name());

	check_file("test/data/atmos_0.mxf", output / "atmos_0.mxf");

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 3U);
	auto reel = dcp.cpls()[0]->reels()[1];
	BOOST_REQUIRE(reel->atmos());
	BOOST_CHECK_EQUAL(reel->atmos()->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(reel->atmos()->actual_duration(), 240);
	BOOST_REQUIRE(reel->atmos()->asset_ref()->file());
	BOOST_REQUIRE(reel->atmos()->asset_ref()->file()->filename() == "atmos_0.mxf");
}


BOOST_AUTO_TEST_CASE(reuse_atmos_for_two_reels_mxf_test)
{
	auto red = content_factory("test/data/flat_red.png")[0];
	auto atmos = make_shared<AtmosMXFContent>("test/data/atmos_0.mxf");
	auto film = new_test_film("reuse_atmos_for_two_reels_mxf_test", { atmos, red });
	film->set_interop(false);

	/* 15s of red with 10s of ATMOS at the end, 3 reels */
	red->video->set_length(24 * 15);
	atmos->set_position(film, dcpomatic::DCPTime::from_seconds(5));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({
		dcpomatic::DCPTime::from_seconds(5),
		dcpomatic::DCPTime::from_seconds(10)
	});

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
		});

	auto const output = film->dir(film->dcp_name());

	check_file("test/data/atmos_0.mxf", output / "atmos_0.mxf");

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 3U);

	{
		auto reel = dcp.cpls()[0]->reels()[0];
		BOOST_REQUIRE(!reel->atmos());
	}
	{

		auto reel = dcp.cpls()[0]->reels()[1];
		BOOST_REQUIRE(reel->atmos());
		BOOST_CHECK_EQUAL(reel->atmos()->entry_point().get_value_or(0), 0);
		BOOST_CHECK_EQUAL(reel->atmos()->actual_duration(), 120);
		BOOST_REQUIRE(reel->atmos()->asset_ref()->file());
		BOOST_REQUIRE_EQUAL(reel->atmos()->asset_ref()->file()->filename(), "atmos_0.mxf");
	}

	{
		auto reel = dcp.cpls()[0]->reels()[2];
		BOOST_REQUIRE(reel->atmos());
		BOOST_CHECK_EQUAL(reel->atmos()->entry_point().get_value_or(0), 120);
		BOOST_CHECK_EQUAL(reel->atmos()->actual_duration(), 120);
		BOOST_REQUIRE(reel->atmos()->asset_ref()->file());
		BOOST_REQUIRE_EQUAL(reel->atmos()->asset_ref()->file()->filename(), "atmos_0.mxf");
	}
}


BOOST_AUTO_TEST_CASE(reuse_two_atmos_assets_with_same_filename)
{
	/* Make a different ATMOS file with the same filename */
	dcp::filesystem::remove_all("build/test/atmos_0.mxf");
	dcp::filesystem::copy_file("test/data/atmos_1.mxf", "build/test/atmos_0.mxf");
	vector<shared_ptr<Content>> atmos = {
		make_shared<AtmosMXFContent>("test/data/atmos_0.mxf"),
		make_shared<AtmosMXFContent>("build/test/atmos_0.mxf")
	};
	auto film = new_test_film("reuse_two_atmos_assets_with_same_filename", atmos);
	film->set_interop(false);

	atmos[0]->set_position(film, dcpomatic::DCPTime::from_seconds(0));
	atmos[1]->set_position(film, dcpomatic::DCPTime::from_seconds(10));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({
		dcpomatic::DCPTime::from_seconds(10),
	});

	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
		});

	auto const output = film->dir(film->dcp_name());

	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	BOOST_REQUIRE_EQUAL(dcp.cpls()[0]->reels().size(), 2U);

	auto reels = dcp.cpls()[0]->reels();

	BOOST_REQUIRE(reels[0]->atmos());
	BOOST_CHECK_EQUAL(reels[0]->atmos()->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(reels[0]->atmos()->actual_duration(), 240);
	BOOST_REQUIRE(reels[0]->atmos()->asset_ref()->file());

	BOOST_REQUIRE(reels[1]->atmos());
	BOOST_CHECK_EQUAL(reels[1]->atmos()->entry_point().get_value_or(0), 0);
	BOOST_CHECK_EQUAL(reels[1]->atmos()->actual_duration(), 240);
	BOOST_REQUIRE(reels[1]->atmos()->asset_ref()->file());

	check_file("test/data/atmos_0.mxf", *reels[0]->atmos()->asset_ref()->file());
	check_file("test/data/atmos_1.mxf", *reels[1]->atmos()->asset_ref()->file());
}


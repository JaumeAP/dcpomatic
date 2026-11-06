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


/** @file  test/atmos_mxf_reuse_test.cc
 *  @brief Test that an Atmos MXF added to a film goes into the DCP as it is when nothing changes it.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/film.h"
#include "test.h"
#include <dcp/atmos_asset.h>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <boost/test/unit_test.hpp>


using std::shared_ptr;


static auto const atmos_file = "test/data/atmos_0.mxf";


/** @return the Atmos reel assets of the first CPL of the DCP that @param film made, one for each reel that has one */
static std::vector<shared_ptr<dcp::ReelAtmosAsset>>
dcp_atmos_assets(shared_ptr<Film> film)
{
	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	std::vector<shared_ptr<dcp::ReelAtmosAsset>> assets;
	for (auto reel: dcp.cpls()[0]->reels()) {
		if (reel->atmos()) {
			assets.push_back(reel->atmos());
		}
	}
	return assets;
}


static shared_ptr<Film>
film_for_atmos_mxf(std::string name, Film::ReuseBehaviour behaviour = Film::ReuseBehaviour::COPY)
{
	auto film = new_test_film(name, content_factory(atmos_file));
	film->set_interop(false);
	film->set_reuse_behaviour(behaviour);
	return film;
}


static void
make_atmos_dcp(shared_ptr<Film> film, std::vector<dcp::VerificationNote::Code> ignore = {})
{
	ignore.push_back(dcp::VerificationNote::Code::MISSING_CPL_METADATA);
	make_and_verify_dcp(film, ignore);
}


/** With ReuseBehaviour::COPY an Atmos MXF that nothing changes goes into the DCP as it is */
BOOST_AUTO_TEST_CASE(atmos_mxf_dcpasset_reused_test)
{
	auto film = film_for_atmos_mxf("atmos_mxf_dcpasset_reused_test");
	make_atmos_dcp(film);

	auto const assets = dcp_atmos_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::AtmosAsset(atmos_file).id());
}


/** The trims of an Atmos MXF that goes into the DCP as it is are the CPL's entry point and duration */
BOOST_AUTO_TEST_CASE(atmos_mxf_dcpasset_trims_are_the_cpl_test)
{
	auto const length = dcp::AtmosAsset(atmos_file).intrinsic_duration();
	BOOST_REQUIRE(length > 30);

	auto film = film_for_atmos_mxf("atmos_mxf_dcpasset_trims_are_the_cpl_test");
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(2, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	make_atmos_dcp(film);

	auto const assets = dcp_atmos_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::AtmosAsset(atmos_file).id());
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 2);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), length - 5);
}


/** An Atmos MXF that fills two reels is used for both, each with its own entry point and duration */
BOOST_AUTO_TEST_CASE(atmos_mxf_dcpasset_two_reels_test)
{
	auto const length = dcp::AtmosAsset(atmos_file).intrinsic_duration();
	BOOST_REQUIRE(length > 60);

	auto film = film_for_atmos_mxf("atmos_mxf_dcpasset_two_reels_test");
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(6, 24));
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_frames(30, 24) });
	make_atmos_dcp(film);

	auto const assets = dcp_atmos_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	for (auto asset: assets) {
		BOOST_CHECK_EQUAL(asset->asset_ref().id(), dcp::AtmosAsset(atmos_file).id());
	}
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 6);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), 30);
	BOOST_CHECK_EQUAL(assets[1]->entry_point().get_value_or(0), 36);
	BOOST_CHECK_EQUAL(assets[1]->actual_duration(), length - 36);
}


/** An Atmos MXF that nothing changes is used as it is whatever the film's re-use behaviour is */
BOOST_AUTO_TEST_CASE(atmos_mxf_dcpasset_is_reused_whatever_the_reuse_behaviour_is_test)
{
	for (auto behaviour: { Film::ReuseBehaviour::RE_ENCODE, Film::ReuseBehaviour::RE_WRAP }) {
		auto film = film_for_atmos_mxf("atmos_mxf_dcpasset_is_reused_whatever_the_reuse_behaviour_is_test_" + std::to_string(static_cast<int>(behaviour)), behaviour);
		make_atmos_dcp(film);

		auto const assets = dcp_atmos_assets(film);
		BOOST_REQUIRE_EQUAL(assets.size(), 1U);
		BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::AtmosAsset(atmos_file).id());
	}
}


/** An Atmos MXF is written again when the film encrypts its sound: the key of the file is not the film's */
BOOST_AUTO_TEST_CASE(atmos_mxf_dcpasset_encrypted_film_is_not_reused_test)
{
	auto film = film_for_atmos_mxf("atmos_mxf_dcpasset_encrypted_film_is_not_reused_test");
	film->set_encrypt_picture(true);
	film->set_encrypt_sound(true);
	film->set_encrypt_text(true);
	make_atmos_dcp(film, { dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED });

	auto const assets = dcp_atmos_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::AtmosAsset(atmos_file).id());
}


/** An Atmos MXF with other Atmos over it is written again */
BOOST_AUTO_TEST_CASE(atmos_mxf_dcpasset_other_atmos_is_not_reused_test)
{
	auto film = film_for_atmos_mxf("atmos_mxf_dcpasset_other_atmos_is_not_reused_test");
	auto other = content_factory("test/data/atmos_1.mxf");
	film->examine_and_add_content(other);
	BOOST_REQUIRE(!wait_for_jobs());
	other[0]->set_position(film, dcpomatic::DCPTime());
	make_atmos_dcp(film);

	auto const assets = dcp_atmos_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::AtmosAsset(atmos_file).id());
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::AtmosAsset("test/data/atmos_1.mxf").id());
}

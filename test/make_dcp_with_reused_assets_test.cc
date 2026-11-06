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


/** @file  test/make_dcp_with_reused_assets_test.cc
 *  @brief Make some DCPs from other DCPs, reusing assets (or not).
 *  @ingroup completedcp
 */


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/text_content.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::shared_ptr;


void check_video_passed_through(shared_ptr<const Film> film)
{
	check_file(
		"test/data/scaling_test_185_185/j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf",
		film->dir(film->dcp_name()) / "j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf"
	);
}


void check_audio_passed_through(shared_ptr<const Film> film)
{
	check_file(
		"test/data/scaling_test_185_185/pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf",
		film->dir(film->dcp_name()) / "pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf"
	);
}


void check_video_not_passed_through(shared_ptr<const Film> film)
{
	BOOST_CHECK(!boost::filesystem::exists(film->dir(film->dcp_name()) / "j2c_cbb4d17a-de58-40f3-bcf9-67c734e70ad7.mxf"));
}


void check_audio_not_passed_through(shared_ptr<const Film> film)
{
	BOOST_CHECK(!boost::filesystem::exists(film->dir(film->dcp_name()) / "pcm_28a37a10-32de-4596-81ae-85bb81197538.mxf"));
}


/** Basic pass-through of picture and sound */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test1)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reuse_dcp_asset_test1", { input });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(8);
	film->set_interop(false);

	make_and_verify_dcp(film);

	check_video_passed_through(film);
	check_audio_passed_through(film);
}


/** The same DCP used twice, as two reels, puts its picture and sound into the new DCP once each */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_same_dcp_in_two_reels_test)
{
	auto first = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto second = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reuse_dcp_asset_same_dcp_in_two_reels_test", { first, second });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film->set_audio_channels(8);
	film->set_interop(false);

	make_and_verify_dcp(film);

	check_video_passed_through(film);
	check_audio_passed_through(film);

	int pictures = 0;
	int sounds = 0;
	for (auto i: boost::filesystem::directory_iterator(film->dir(film->dcp_name()))) {
		auto const name = i.path().filename().string();
		pictures += name.substr(0, 4) == "j2c_" ? 1 : 0;
		sounds += name.substr(0, 4) == "pcm_" ? 1 : 0;
	}
	BOOST_CHECK_EQUAL(pictures, 1);
	BOOST_CHECK_EQUAL(sounds, 1);
}


/** No pass-through with the wrong standard */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test2)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reuse_dcp_asset_test2", { input });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(8);
	film->set_interop(true);

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::INVALID_STANDARD });

	check_video_not_passed_through(film);
	check_audio_not_passed_through(film);
}



/** No audio pass-through with the wrong channel count */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test3)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto film = new_test_film("reuse_dcp_asset_test3", { input });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_audio_channels(6);
	film->set_interop(false);

	make_and_verify_dcp(film);

	check_video_passed_through(film);
	check_audio_not_passed_through(film);
}


/** Video pass-through with non-burnt subtitles */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test4)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto subs = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("reuse_dcp_asset_test4", { input, subs });
	subs->text[0]->set_burn(false);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film->set_audio_channels(8);
	film->set_interop(false);

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE, dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME });

	check_video_passed_through(film);
	check_audio_passed_through(film);
}


/** Video not passed through with burnt subtitles */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_test5)
{
	auto input = std::make_shared<DCPContent>("test/data/scaling_test_185_185");
	auto subs = content_factory("test/data/15s.srt")[0];
	auto film = new_test_film("reuse_dcp_asset_test4", { input, subs });
	subs->text[0]->set_burn(true);
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	film->set_audio_channels(8);
	film->set_interop(false);

	make_and_verify_dcp(film);

	check_video_not_passed_through(film);
	check_audio_passed_through(film);
}


/** The assets of a DCP that has one reel are not re-used in a film that splits that reel in two: the asset's period is
 *  not a reel of the film, and a trim in the input would put the entry points in the wrong place
 */
BOOST_AUTO_TEST_CASE(reuse_dcp_asset_one_reel_split_in_two_is_not_reused_test)
{
	auto still = content_factory("test/data/flat_red.png");
	auto source = new_test_film("reuse_dcp_asset_one_reel_split_in_two_is_not_reused_test_source", still);
	still[0]->video->set_length(72);
	make_and_verify_dcp(
		source,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});
	auto const source_picture = find_file(source->dir(source->dcp_name()), "j2c_").filename();

	auto input = std::make_shared<DCPContent>(source->dir(source->dcp_name()));
	auto film = new_test_film("reuse_dcp_asset_one_reel_split_in_two_is_not_reused_test", { input });
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ dcpomatic::DCPTime::from_frames(36, 24) });
	film->set_interop(false);

	make_and_verify_dcp(film, { dcp::VerificationNote::Code::MISSING_CPL_METADATA });

	BOOST_CHECK(!boost::filesystem::exists(film->dir(film->dcp_name()) / source_picture));
}

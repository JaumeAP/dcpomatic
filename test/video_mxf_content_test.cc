/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/video_mxf_content_test.cc
 *  @brief Test use of Video MXF content.
 *  @ingroup feature
 */



#include "lib/content_factory.h"
#include "lib/dcp_content_type.h"
#include "lib/decoder.h"
#include "lib/decoder_factory.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "lib/video_decoder.h"
#include "lib/video_mxf_content.h"
#include "test.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/equality_options.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <boost/test/unit_test.hpp>


using std::dynamic_pointer_cast;
using std::make_shared;


static void note (dcp::NoteType, std::string)
{

}


/** Basic test of using video MXF content */
BOOST_AUTO_TEST_CASE (video_mxf_content_test)
{
	auto const ref_mxf = find_file("test/data/scaling_test_185_185", "j2c");
	auto content = content_factory(ref_mxf);

	auto film = new_test_film("video_mxf_content_test", content);
	make_and_verify_dcp (
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K
		});

	auto ref = make_shared<dcp::MonoJ2KPictureAsset>(ref_mxf);
	auto comp_mxf = find_file(film->file(film->dcp_name()), "j2c_");
	auto comp = make_shared<dcp::MonoJ2KPictureAsset>(comp_mxf);
	dcp::EqualityOptions op;
	BOOST_CHECK (ref->equals (comp, op, note));
}


/** A picture MXF decoder whose video is ignored reads no frames: it says at once that it has finished */
BOOST_AUTO_TEST_CASE(video_mxf_decoder_ignore_test)
{
	auto content = content_factory(find_file("test/data/scaling_test_185_185", "j2c"));
	auto film = new_test_film("video_mxf_decoder_ignore_test", content);
	auto decoder = decoder_factory(film, content[0], false, false, {});
	BOOST_REQUIRE(decoder->video);
	decoder->video->set_ignore(true);
	BOOST_CHECK(decoder->pass());
}


/** Make a DCP of 3 seconds of a still image and @return the path of its picture MXF.  The film's minimum length is
 *  1 second, so a picture MXF to be trimmed has to be longer than that.
 */
static boost::filesystem::path
make_picture_mxf(std::string name, boost::filesystem::path image)
{
	auto content = content_factory(image);
	auto film = new_test_film(name, content);
	content[0]->video->set_length(72);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});
	return find_file(film->dir(film->dcp_name()), "j2c_");
}


/** @return a new film that suits the picture MXF @param mxf, with COPY as its re-use behaviour and the file added to it */
static std::shared_ptr<Film>
film_for_picture_mxf(std::string name, boost::filesystem::path mxf, Film::ReuseBehaviour behaviour = Film::ReuseBehaviour::COPY)
{
	auto film = new_test_film(name);
	film->set_interop(dcp::MonoJ2KPictureAsset(mxf).standard() == dcp::Standard::INTEROP);
	film->set_reuse_behaviour(behaviour);
	film->examine_and_add_content(content_factory(mxf));
	BOOST_REQUIRE(!wait_for_jobs());
	return film;
}


static void
make_picture_only_dcp(std::shared_ptr<Film> film)
{
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA
		});
}


/** @return the picture reel assets of the first CPL of the DCP that @param film made, one for each reel */
static std::vector<std::shared_ptr<dcp::ReelPictureAsset>>
dcp_picture_assets(std::shared_ptr<Film> film)
{
	dcp::DCP dcp(film->dir(film->dcp_name()));
	dcp.read();
	BOOST_REQUIRE_EQUAL(dcp.cpls().size(), 1U);
	std::vector<std::shared_ptr<dcp::ReelPictureAsset>> assets;
	for (auto reel: dcp.cpls()[0]->reels()) {
		BOOST_REQUIRE(reel->main_picture());
		assets.push_back(reel->main_picture());
	}
	return assets;
}


/** With ReuseBehaviour::COPY a picture MXF that only a trim changes goes into the DCP as it is, and the CPL makes the trim */
BOOST_AUTO_TEST_CASE(picture_mxf_dcpasset_reused_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_dcpasset_reused_test_source", "test/data/flat_red.png");
	auto film = film_for_picture_mxf("picture_mxf_dcpasset_reused_test", mxf);
	film->content()[0]->set_trim_start(dcpomatic::ContentTime::from_frames(2, 24));
	film->content()[0]->set_trim_end(dcpomatic::ContentTime::from_frames(3, 24));
	make_picture_only_dcp(film);

	auto const source_length = dcp::MonoJ2KPictureAsset(mxf).intrinsic_duration();
	auto const assets = dcp_picture_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::MonoJ2KPictureAsset(mxf).id());
	BOOST_CHECK_EQUAL(assets[0]->entry_point().get_value_or(0), 2);
	BOOST_CHECK_EQUAL(assets[0]->actual_duration(), source_length - 5);
}


/** A picture MXF that the film crops is written again */
BOOST_AUTO_TEST_CASE(picture_mxf_dcpasset_crop_is_not_reused_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_dcpasset_crop_is_not_reused_test_source", "test/data/flat_red.png");
	auto film = film_for_picture_mxf("picture_mxf_dcpasset_crop_is_not_reused_test", mxf);
	film->content()[0]->video->set_left_crop(10);
	make_picture_only_dcp(film);

	auto const assets = dcp_picture_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::MonoJ2KPictureAsset(mxf).id());
}


/** A picture MXF whose frame type is a 3D layout in a 2D film is written again */
BOOST_AUTO_TEST_CASE(picture_mxf_dcpasset_3d_frame_type_is_not_reused_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_dcpasset_3d_frame_type_is_not_reused_test_source", "test/data/flat_red.png");
	auto film = film_for_picture_mxf("picture_mxf_dcpasset_3d_frame_type_is_not_reused_test", mxf);
	film->content()[0]->video->set_frame_type(VideoFrameType::THREE_D_LEFT_RIGHT);
	make_picture_only_dcp(film);

	auto const assets = dcp_picture_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::MonoJ2KPictureAsset(mxf).id());
}


/** A picture MXF is written again when the film does not copy */
BOOST_AUTO_TEST_CASE(picture_mxf_dcpasset_re_wrap_is_not_reused_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_dcpasset_re_wrap_is_not_reused_test_source", "test/data/flat_red.png");
	auto film = film_for_picture_mxf("picture_mxf_dcpasset_re_wrap_is_not_reused_test", mxf, Film::ReuseBehaviour::RE_WRAP);
	make_picture_only_dcp(film);

	auto const assets = dcp_picture_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::MonoJ2KPictureAsset(mxf).id());
}


/** A picture MXF with other video over it is written again */
BOOST_AUTO_TEST_CASE(picture_mxf_dcpasset_other_video_is_not_reused_test)
{
	auto const mxf = make_picture_mxf("picture_mxf_dcpasset_other_video_is_not_reused_test_source", "test/data/flat_red.png");
	auto film = film_for_picture_mxf("picture_mxf_dcpasset_other_video_is_not_reused_test", mxf);
	/* The still is as long as the picture MXF and starts with it, so the reel is the MXF's period and only the overlap
	 * can stop the MXF being re-used.
	 */
	auto other = content_factory("test/data/flat_red.png");
	film->examine_and_add_content(other);
	BOOST_REQUIRE(!wait_for_jobs());
	other[0]->video->set_length(72);
	other[0]->set_position(film, dcpomatic::DCPTime());
	make_picture_only_dcp(film);

	auto const assets = dcp_picture_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 1U);
	BOOST_CHECK(assets[0]->asset_ref().id() != dcp::MonoJ2KPictureAsset(mxf).id());
}


/** An encrypted picture MXF is never used as it is: the DCP's key is not its key */
BOOST_AUTO_TEST_CASE(picture_mxf_dcpasset_encrypted_is_not_reused_test)
{
	auto content = content_factory("test/data/flat_red.png");
	auto source = new_test_film("picture_mxf_dcpasset_encrypted_is_not_reused_test_source", content);
	content[0]->video->set_length(72);
	source->set_encrypt_picture(true);
	make_and_verify_dcp(
		source,
		{
			dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE,
			dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE,
			dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K,
			dcp::VerificationNote::Code::MISSING_CPL_METADATA,
			dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED
		});
	auto const mxf = find_file(source->dir(source->dcp_name()), "j2c_");

	auto film = new_test_film("picture_mxf_dcpasset_encrypted_is_not_reused_test");
	film->set_reuse_behaviour(Film::ReuseBehaviour::COPY);
	film->examine_and_add_content(content_factory(mxf));
	/* If the encrypted file is refused when it is examined that is fine; if not, it must not be re-used */
	if (!wait_for_jobs()) {
		BOOST_REQUIRE(!film->content().empty());
		make_picture_only_dcp(film);
		auto const assets = dcp_picture_assets(film);
		BOOST_REQUIRE_EQUAL(assets.size(), 1U);
		BOOST_CHECK(assets[0]->asset_ref().id() != dcp::MonoJ2KPictureAsset(mxf).id());
	}
}


/** Two different picture MXFs with the same file name, one in each of two reels, are both in the DCP */
BOOST_AUTO_TEST_CASE(picture_mxf_dcpasset_same_file_name_test)
{
	auto copy_as_picture_mxf = [](boost::filesystem::path from, std::string directory) {
		auto const dir = boost::filesystem::path("build") / "test" / directory;
		boost::filesystem::remove_all(dir);
		boost::filesystem::create_directories(dir);
		boost::filesystem::copy_file(from, dir / "picture.mxf");
		return dir / "picture.mxf";
	};

	auto const first = copy_as_picture_mxf(
		make_picture_mxf("picture_mxf_dcpasset_same_file_name_test_source1", "test/data/flat_red.png"), "picture_mxf_dcpasset_same_file_name_test_dir1"
		);
	auto const second = copy_as_picture_mxf(
		make_picture_mxf("picture_mxf_dcpasset_same_file_name_test_source2", "test/data/flat_green.png"), "picture_mxf_dcpasset_same_file_name_test_dir2"
		);
	BOOST_REQUIRE(dcp::MonoJ2KPictureAsset(first).id() != dcp::MonoJ2KPictureAsset(second).id());

	auto film = film_for_picture_mxf("picture_mxf_dcpasset_same_file_name_test", first);
	auto content = content_factory(second);
	film->examine_and_add_content(content);
	BOOST_REQUIRE(!wait_for_jobs());
	auto const length = film->content()[0]->full_length(film);
	content[0]->set_position(film, length);
	film->set_reel_type(ReelType::CUSTOM);
	film->set_custom_reel_boundaries({ length });
	make_picture_only_dcp(film);

	auto const assets = dcp_picture_assets(film);
	BOOST_REQUIRE_EQUAL(assets.size(), 2U);
	BOOST_CHECK_EQUAL(assets[0]->asset_ref().id(), dcp::MonoJ2KPictureAsset(first).id());
	BOOST_CHECK_EQUAL(assets[1]->asset_ref().id(), dcp::MonoJ2KPictureAsset(second).id());
}

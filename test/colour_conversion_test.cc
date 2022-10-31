/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/colour_conversion_test.cc
 *  @brief Test ColourConversion class.
 *  @ingroup selfcontained
 */


#include "lib/butler.h"
#include "lib/colour_conversion.h"
#include "lib/content_factory.h"
#include "lib/dcp_video.h"
#include "lib/film.h"
#include "lib/player.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/gamma_transfer_function.h>
#include <dcp/openjpeg_image.h>
#include <dcp/j2k_transcode.h>
#include <libxml++/libxml++.h>
#include <boost/test/unit_test.hpp>
#include <iostream>


using std::cout;
using std::make_shared;
using std::shared_ptr;
using std::vector;


BOOST_AUTO_TEST_CASE (colour_conversion_test1)
{
	ColourConversion A (dcp::ColourConversion::srgb_to_xyz());
	ColourConversion B (dcp::ColourConversion::rec709_to_xyz());

	BOOST_CHECK_EQUAL (A.identifier(), "9840c601d2775bf1b3847254bbaa36a9");
	BOOST_CHECK_EQUAL (B.identifier(), "58151ac92fdf333663a62c9a8ba5c5f4");
}


BOOST_AUTO_TEST_CASE (colour_conversion_test2)
{
	ColourConversion A (dcp::ColourConversion::srgb_to_xyz ());
	xmlpp::Document doc;
	auto root = doc.create_root_node ("Test");
	A.as_xml (root);
	BOOST_CHECK_EQUAL (
		doc.write_to_string_formatted ("UTF-8"),
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Test>\n"
		"  <InputTransferFunction>\n"
		"    <Type>ModifiedGamma</Type>\n"
		"    <Power>2.4</Power>\n"
		"    <Threshold>0.04045</Threshold>\n"
		"    <A>0.055</A>\n"
		"    <B>12.92</B>\n"
		"  </InputTransferFunction>\n"
		"  <YUVToRGB>0</YUVToRGB>\n"
		"  <RedX>0.64</RedX>\n"
		"  <RedY>0.33</RedY>\n"
		"  <GreenX>0.3</GreenX>\n"
		"  <GreenY>0.6</GreenY>\n"
		"  <BlueX>0.15</BlueX>\n"
		"  <BlueY>0.06</BlueY>\n"
		"  <WhiteX>0.3127</WhiteX>\n"
		"  <WhiteY>0.329</WhiteY>\n"
		"  <OutputGamma>2.6</OutputGamma>\n"
		"</Test>\n"
		);
}


BOOST_AUTO_TEST_CASE (colour_conversion_test3)
{
	ColourConversion A (dcp::ColourConversion::rec709_to_xyz());
	xmlpp::Document doc;
	auto root = doc.create_root_node ("Test");
	A.as_xml (root);
	BOOST_CHECK_EQUAL (
		doc.write_to_string_formatted ("UTF-8"),
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<Test>\n"
		"  <InputTransferFunction>\n"
		"    <Type>Gamma</Type>\n"
		"    <Gamma>2.2</Gamma>\n"
		"  </InputTransferFunction>\n"
		"  <YUVToRGB>1</YUVToRGB>\n"
		"  <RedX>0.64</RedX>\n"
		"  <RedY>0.33</RedY>\n"
		"  <GreenX>0.3</GreenX>\n"
		"  <GreenY>0.6</GreenY>\n"
		"  <BlueX>0.15</BlueX>\n"
		"  <BlueY>0.06</BlueY>\n"
		"  <WhiteX>0.3127</WhiteX>\n"
		"  <WhiteY>0.329</WhiteY>\n"
		"  <OutputGamma>2.6</OutputGamma>\n"
		"</Test>\n"
		);
}


/** Test a round trip via the XML representation */
BOOST_AUTO_TEST_CASE (colour_conversion_test4)
{
	for (auto const& i: PresetColourConversion::all()) {
		xmlpp::Document out;
		auto out_root = out.create_root_node("Test");
		i.conversion.as_xml (out_root);
		auto in = make_shared<cxml::Document> ("Test");
		in->read_string (out.write_to_string("UTF-8"));
		BOOST_CHECK (ColourConversion::from_xml(in, Film::current_state_version).get() == i.conversion);
	}
}


BOOST_AUTO_TEST_CASE(noisy_blacks_test)
{
	auto content = content_factory(TestPaths::private_data() / "island.mov").front();
	auto film = new_test_film2("noisy_black_test", { content });

	Player player(film, Image::Alignment::COMPACT);
	player.set_fast();
	player.set_always_burn_open_subtitles();
	player.set_play_referenced();

	auto butler = make_shared<Butler>(
		film, player, AudioMapping(), 2, boost::bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::PADDED, true, false, Butler::Audio::DISABLED
		);

	auto video = butler->get_video(Butler::Behaviour::BLOCKING).first;

	vector<int> bitrates = { 50, 100, 150, 200, 250 };

	for (auto bitrate: bitrates) {
		auto before_encode = DCPVideo::convert_to_xyz(video, [](dcp::NoteType, std::string) {});
		write_openjpeg_image(before_encode, String::compose("build/test/noisy_black_test_%1_before.opj", bitrate));
		auto encoded = DCPVideo(video, 0, 24, bitrate * 1000000LL, Resolution::FOUR_K).encode_locally();
		auto after_decode = dcp::decompress_j2k(encoded, 0);
		write_openjpeg_image(after_decode, String::compose("build/test/noisy_black_test_%1_after.opj", bitrate));

		int max_diffs[] = { 0, 0, 0 };

		int const width = video->out_size().width;
		int const height = video->out_size().height;

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				int offset = y * width + x;
				for (int c = 0; c < 3; ++c) {
					auto const diff = std::abs(before_encode->data(c)[offset] - after_decode->data(c)[offset]);
					max_diffs[c] = std::max(max_diffs[c], diff);
				}
			}
		}

		std::cout << bitrate << "MBps diffs; " << max_diffs[0] << " " << max_diffs[1] << " " << max_diffs[2] << "\n";
	}
}


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


#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/image_jpeg.h"
#include "lib/image_png.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/video_content.h"
#include <dcp/openjpeg_image.h>
#include <dcp/j2k_transcode.h>
#include <dcp/rgb_xyz.h>
#include "test.h"
#include <boost/test/unit_test.hpp>


static
int
max_rgb_difference(std::shared_ptr<Image> image)
{
	BOOST_REQUIRE(image->pixel_format() == AV_PIX_FMT_RGB24);

	int max = 0;

	auto const width = image->size().width;
	auto const height = image->size().height;
	auto const stride = image->stride()[0];

	uint8_t* row = image->data()[0];
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			uint8_t* pixel = row + x * 3;
			max = std::max(max, std::abs(static_cast<int>(pixel[0]) - static_cast<int>(pixel[1])));
			max = std::max(max, std::abs(static_cast<int>(pixel[0]) - static_cast<int>(pixel[2])));
			max = std::max(max, std::abs(static_cast<int>(pixel[1]) - static_cast<int>(pixel[2])));
		}
		row += stride;
	}

	return max;
}


BOOST_AUTO_TEST_CASE(monochrome_test)
{
	auto source = TestPaths::private_data() / "mahler_mix_photos_2024_04_002174.jpg";
	auto content = content_factory(source);
	BOOST_REQUIRE(!content.empty());
	auto film = new_test_film("monochrome_test", content);
	make_and_verify_dcp(film);

	auto player = std::make_shared<Player>(film, Image::Alignment::PADDED, false);

	std::shared_ptr<PlayerVideo> video;
	player->Video.connect([&](std::shared_ptr<PlayerVideo> v, dcpomatic::DCPTime) {
		video = v;
	});

	while (!video) {
		BOOST_REQUIRE(!player->pass());
	}

	auto pre_image = video->image(boost::bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, false);

	std::cout << "CC " << content[0]->video->colour_conversion()->preset().get() << "\n";

	auto dcp = std::make_shared<DCPContent>(film->dir(film->dcp_name()));
	auto film2 = new_test_film("monochrome_test_reload", { dcp });
	auto player2 = std::make_shared<Player>(film2, Image::Alignment::PADDED, false);

	std::shared_ptr<PlayerVideo> video2;
	player2->Video.connect([&](std::shared_ptr<PlayerVideo> v, dcpomatic::DCPTime) {
		video2 = v;
	});

	while (!video2) {
		BOOST_REQUIRE(!player2->pass());
	}

	auto image = video2->image(boost::bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, false);

	auto j2k = dcp::decompress_j2k(*video2->j2k(), 0);

	int max[3] = { 0, 0, 0 };
	for (int y = 0; y < j2k->size().height; ++y) {
		for (int x = 0; x < j2k->size().width; ++x) {
			for (int c = 0; c < 3; ++c) {
				max[c] = std::max(max[c], j2k->data(c)[y * j2k->size().width + x]);
			}
		}
	}

	std::cout << "J2K max " << max[0] << " " << max[1] << " " << max[2] << "\n";

	auto rgb_image_16 = std::make_shared<Image>(AV_PIX_FMT_RGB48LE, j2k->size(), Image::Alignment::COMPACT);

	dcp::xyz_to_rgb(j2k, dcp::ColourConversion::srgb_to_xyz(), rgb_image_16->data()[0], j2k->size().width * 6);

	auto rgb_image = std::make_shared<Image>(AV_PIX_FMT_RGB24, j2k->size(), Image::Alignment::COMPACT);
	for (int y = 0; y < j2k->size().height; ++y) {
		uint16_t* from = reinterpret_cast<uint16_t*>(rgb_image_16->data()[0] + y * rgb_image_16->stride()[0]);
		uint8_t* to = rgb_image->data()[0] + y * rgb_image->stride()[0];
		for (int x = 0; x < j2k->size().width; ++x) {
			*to++ = *from++ >> 8;
			*to++ = *from++ >> 8;
			*to++ = *from++ >> 8;
		}
	}

	image_as_png(image).write("build/test/monochrome_test_1.png");
	image_as_png(rgb_image).write("build/test/monochrome_test_2.png");

	std::cout << "start " << max_rgb_difference(image_from_jpeg(source, Image::Alignment::COMPACT)) << "\n";
	std::cout << "pre   " << max_rgb_difference(pre_image) << "\n";
	std::cout << "end   " << max_rgb_difference(image) << "\n";
	std::cout << "dcp   " << max_rgb_difference(rgb_image) << "\n";
}


BOOST_AUTO_TEST_CASE(monochrome_test2)
{
	auto source = TestPaths::private_data() / "mahler_mix_photos_2024_04_002174.jpg";

	auto A = image_from_jpeg(source, Image::Alignment::PADDED);
	auto B = A->convert_pixel_format(dcp::YUVToRGB::REC709, AV_PIX_FMT_RGB48LE, Image::Alignment::PADDED, false);
	auto C = dcp::rgb_to_xyz(B->data()[0], B->size(), B->stride()[0], dcp::ColourConversion::srgb_to_xyz());

	// auto comp = dcp::compress_j2k(C, 250000000, 24, false, false);
	// auto dec = dcp::decompress_j2k(comp, 0);

	auto D = std::make_shared<Image>(AV_PIX_FMT_RGB48LE, C->size(), Image::Alignment::COMPACT);
	dcp::xyz_to_rgb(C, dcp::ColourConversion::srgb_to_xyz(), D->data()[0], A->size().width * 6);
	// dcp::xyz_to_rgb(dec, dcp::ColourConversion::srgb_to_xyz(), D->data()[0], A->size().width * 6);

	auto E = std::make_shared<Image>(AV_PIX_FMT_RGB24, A->size(), Image::Alignment::COMPACT);
	for (int y = 0; y < A->size().height; ++y) {
		uint16_t* from = reinterpret_cast<uint16_t*>(D->data()[0] + y * D->stride()[0]);
		uint8_t* to = E->data()[0] + y * E->stride()[0];
		for (int x = 0; x < A->size().width; ++x) {
			*to++ = *from++ >> 8;
			*to++ = *from++ >> 8;
			*to++ = *from++ >> 8;
		}
	}

	std::cout << "start " << max_rgb_difference(image_from_jpeg(source, Image::Alignment::PADDED)) << "\n";
	std::cout << "end   " << max_rgb_difference(E) << "\n";

}


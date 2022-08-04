/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "lib/colour_conversion.h"
#include "lib/j2k_image_proxy.h"
#include "lib/player_video.h"
#include <dcp/openjpeg_image.h>
#include <dcp/j2k_transcode.h>
#include <dcp/mono_picture_frame.h>
#include <boost/optional.hpp>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::weak_ptr;
using boost::optional;


BOOST_AUTO_TEST_CASE(player_video_j2k_passthrough_test)
{
	auto size = dcp::Size(4096, 2048);
	auto image = make_shared<dcp::OpenJPEGImage>(size);
	for (int x = 0; x < size.width; ++x) {
		image->data(0)[x] = x;
		image->data(1)[x] = x;
		image->data(2)[x] = x;
	}
	for (int y = 1; y < size.height; ++y) {
		memcpy(image->data(0) + y * size.width, image->data(0), size.width);
		memcpy(image->data(1) + y * size.width, image->data(1), size.width);
		memcpy(image->data(2) + y * size.width, image->data(2), size.width);
	}
	auto compressed = compress_j2k(image, 250000000LL, 24, false, true);
	auto decompressed = decompress_j2k(compressed, 0);
	auto frame = make_shared<dcp::MonoPictureFrame>(compressed.data(), compressed.size());
	auto proxy = make_shared<J2KImageProxy>(frame, size, AV_PIX_FMT_XYZ12LE, optional<int>());
	auto video = make_shared<PlayerVideo>(proxy, Crop(), optional<double>(), size, size, Eyes::BOTH, Part::WHOLE, optional<ColourConversion>(), VideoRange::FULL, weak_ptr<Content>(), optional<Frame>(), false);
	auto out_image = video->image([](AVPixelFormat) { return AV_PIX_FMT_XYZ12LE; }, VideoRange::FULL, false);

	BOOST_REQUIRE_EQUAL(out_image->size().width, size.width);
	BOOST_REQUIRE_EQUAL(out_image->size().height, size.height);

	for (int c = 0; c < 3; ++c) {
		for (auto y = 0; y < size.height; ++y) {
			auto in = decompressed->data(c) + y * size.width;
			auto out = reinterpret_cast<uint16_t*>(out_image->data()[0] + y * out_image->stride()[0]) + c;
			for (int x = 0; x < size.width; ++x) {
				BOOST_REQUIRE_MESSAGE(*in == (*out >> 4), *in << " != " << *out << " at x=" << x << ", y=" << y << ", c=" << c);
				++in;
				out += 3;
			}
		}
	}
}


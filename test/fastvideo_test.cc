/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/barrier.h"
#include "lib/colour_conversion.h"
#include "lib/dcp_video.h"
#include "lib/ffmpeg_image_proxy.h"
#include "lib/player_video.h"
#include "lib/j2k_encoder_fastvideo_backend.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE (fastvideo_frame_size_test)
{
	auto test = [](int j2k_bandwidth) {
		Barrier barrier (1);
		J2KEncoderFastvideoBackend backend (barrier);

		auto image_proxy = std::make_shared<FFmpegImageProxy>(TestPaths::private_data() / "prophet_frame.tiff");
		auto player_video = std::make_shared<PlayerVideo> (
			image_proxy,
			Crop(),
			boost::optional<double>(),
			dcp::Size(1998, 1080),
			dcp::Size(1998, 1080),
			Eyes::BOTH,
			Part::WHOLE,
			PresetColourConversion::from_id("rec709").conversion,
			VideoRange::FULL,
			std::weak_ptr<Content>(),
			boost::optional<Frame>(),
			false
			);

		DCPVideo dcp_video (
			player_video,
			0,
			24,
			j2k_bandwidth,
			Resolution::TWO_K
			);

		auto result = backend.encode ({dcp_video});
		return result[0].size() * 8L * 24 / 1000000.0f;
	};

	auto bandwidth = std::vector<int>{ 50, 100, 150, 200, 250 };

	for (auto b: bandwidth) {
		auto mbps = test(b * 1000000.0f);
		BOOST_CHECK_CLOSE (mbps, b, 5);
	}
}


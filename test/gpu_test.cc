/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/gpu_j2k_encode_worker.h"
#include "lib/ffmpeg_image_proxy.h"
#include "lib/player_video.h"
#include "lib/dcp_video.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;
using boost::optional;
using boost::weak_ptr;

BOOST_AUTO_TEST_CASE (gpu_test)
{
	GPUJ2KEncodeWorker w;
	shared_ptr<ImageProxy> ip (new FFmpegImageProxy(TestPaths::private_data / "card.png"));
	shared_ptr<PlayerVideo> pv (
		new PlayerVideo(
			ip, Crop(), optional<double>(), dcp::Size(1998, 1080), dcp::Size(1998, 1080),
			EYES_BOTH, PART_WHOLE, optional<ColourConversion>(), VIDEO_RANGE_FULL, weak_ptr<Content>(), optional<Frame>(), false
			)
		);
	shared_ptr<DCPVideo> vf (new DCPVideo(pv, 0, 24, 100000000, RESOLUTION_2K));
	optional<dcp::Data> encoded = w.encode (vf);
	FILE* f = fopen("titz.j2k", "wb");
	fwrite(encoded->data().get(), encoded->size(), 1, f);
	fclose(f);
}

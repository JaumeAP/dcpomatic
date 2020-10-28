/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "lib/compose.hpp"
#include "lib/image.h"
#include "lib/timer.h"


using boost::shared_ptr;


#define ITERATIONS 500


static
shared_ptr<Image>
dummy_image(dcp::Size size, AVPixelFormat pixel)
{
	shared_ptr<Image> image(new Image(pixel, size, true));
	int v = 0;
	for (int y = 0; y < size.height; ++y) {
		for (int c = 0; c < image->planes(); ++c) {
			uint8_t* p = image->data()[c] + y * image->stride()[c];
			int const line_size = image->line_size()[c];
			for (int x = 0; x < line_size; ++x) {
				*p++ = v++ % 256;
			}
		}
	}

	return image;
}


static
void
test (dcp::Size from_size, AVPixelFormat from_pixel, dcp::Size to, bool fast)
{
	shared_ptr<Image> image = dummy_image(from_size, from_pixel);
	PeriodTimer pt (String::compose("%1:%2 -> %3:%4 %5 %6", from_size.width, from_size.height, to.width, to.height, static_cast<int>(from_pixel), fast ? "fast" : "slow"));
	for (int i = 0; i < ITERATIONS; ++i) {
		image->scale (to, dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGB48, true, fast);
	}
}


int
main ()
{
	test (dcp::Size(1998, 1080), AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), true);
	test (dcp::Size(1998, 1080), AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), false);

	test (dcp::Size(1998, 1080), AV_PIX_FMT_RGB24, dcp::Size(2006, 1088), true);
	test (dcp::Size(1998, 1080), AV_PIX_FMT_RGB24, dcp::Size(2006, 1088), false);

	test (dcp::Size(996, 540), AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), true);
	test (dcp::Size(996, 540), AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), false);

	test (dcp::Size(498, 270), AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), true);
	test (dcp::Size(498, 270), AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), false);

	test (dcp::Size(1998, 1080), AV_PIX_FMT_XYZ12LE, dcp::Size(1998, 1080), true);
	test (dcp::Size(1998, 1080), AV_PIX_FMT_XYZ12LE, dcp::Size(1998, 1080), false);

	test (dcp::Size(1998, 1080), AV_PIX_FMT_XYZ12LE, dcp::Size(2006, 1088), true);
	test (dcp::Size(1998, 1080), AV_PIX_FMT_XYZ12LE, dcp::Size(2006, 1088), false);

	test (dcp::Size(996, 540), AV_PIX_FMT_XYZ12LE, dcp::Size(1998, 1080), true);
	test (dcp::Size(996, 540), AV_PIX_FMT_XYZ12LE, dcp::Size(1998, 1080), false);

	test (dcp::Size(498, 270), AV_PIX_FMT_XYZ12LE, dcp::Size(1998, 1080), true);
	test (dcp::Size(498, 270), AV_PIX_FMT_XYZ12LE, dcp::Size(1998, 1080), false);

	return 0;
}


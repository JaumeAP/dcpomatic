/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_BITMAP_CAPTION_H
#define DCPOMATIC_BITMAP_CAPTION_H


#include "rect.h"
#include <dcp/types.h>
#include <memory>


class Image;

class BitmapText
{
public:
	BitmapText(dcp::Size parent_size_, std::shared_ptr<const Image> image_, dcpomatic::Rect<double> rectangle_)
		: parent_size(parent_size_)
		, image(image_)
		, rectangle(rectangle_)
	{}

	/** Size of the image that this subtitle has come from, with any crop already applied */
	dcp::Size parent_size;
	std::shared_ptr<const Image> image;
	/** Area that the subtitle covers on its corresponding video, expressed in
	 *  proportions of the image size; e.g. rectangle.x = 0.5 would mean that
	 *  the rectangle starts half-way across the video.
	 *
	 *  This rectangle may or may not have had a TextContent's offsets and
	 *  scale applied to it, depending on context.
	 */
	dcpomatic::Rect<double> rectangle;
};

#endif

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


#include "content.h"
#include "dcpomatic_time.h"
#include "video_content.h"
#include <iostream>
#include <string>


class BlackContent : public Content
{
public:
	BlackContent (dcpomatic::DCPTimePeriod period, Eyes eyes)
		: Content(period.from)
		, _length(period.duration())
	{
		video = std::make_shared<VideoContent>(this);
		if (eyes == Eyes::BOTH) {
			video->set_frame_type(VideoFrameType::TWO_D);
		} else if (eyes == Eyes::LEFT) {
			video->set_frame_type(VideoFrameType::THREE_D_LEFT);
		} else if (eyes == Eyes::RIGHT) {
			video->set_frame_type(VideoFrameType::THREE_D_RIGHT);
		}
	}

	std::string summary () const override {
		return "black";
	}

	dcpomatic::DCPTime full_length (std::shared_ptr<const Film>) const override {
		return _length;
	}

	dcpomatic::DCPTime approximate_length () const override {
		return _length;
	}

private:
	dcpomatic::DCPTime _length;
};


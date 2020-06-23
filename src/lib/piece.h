/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_PIECE_H
#define DCPOMATIC_PIECE_H

#include "audio_stream.h"
#include "content.h"
#include "dcpomatic_time.h"
#include "frame_rate_change.h"
#include "types.h"
#include "video_content.h"

class Content;
class Decoder;
namespace dcpomatic {
	class Font;
}

class Piece
{
public:
	Piece (boost::shared_ptr<Content> c, boost::shared_ptr<Decoder> d, FrameRateChange f)
		: decoder (d)
		, frc (f)
		, done (false)
		, _content (c)
	{}

	dcpomatic::DCPTime position () const;
	dcpomatic::DCPTime end (boost::shared_ptr<const Film> film) const;
	dcpomatic::DCPTime content_video_to_dcp (Frame f) const;
	dcpomatic::DCPTime content_time_to_dcp (dcpomatic::ContentTime t) const;
	boost::optional<dcpomatic::DCPTime> content_time_to_dcp (boost::shared_ptr<const Content> content, dcpomatic::ContentTime t);
	dcpomatic::DCPTime resampled_audio_to_dcp (boost::shared_ptr<const Film> film, Frame f) const;
	dcpomatic::ContentTime dcp_to_content_time (boost::shared_ptr<const Film> film, dcpomatic::DCPTime t) const;

	dcpomatic::ContentTime trim_start () const;
	std::list<boost::shared_ptr<dcpomatic::Font> > fonts () const;
	bool referenced_audio () const;

	Crop video_crop () const;
	bool video_use () const;
	boost::optional<double> video_fade (boost::shared_ptr<const Film> film, Frame frame) const;
	dcp::Size video_scaled_size (dcp::Size container_size);
	boost::optional<ColourConversion> video_colour_conversion () const;
	VideoRange video_range () const;

	int audio_resampled_frame_rate (boost::shared_ptr<const Film> film) const;
	double audio_gain () const;
	std::vector<AudioStreamPtr> audio_streams () const;

	boost::shared_ptr<Decoder> decoder;
	FrameRateChange frc;
	bool done;

private:
	boost::shared_ptr<Content> _content;
};

#endif

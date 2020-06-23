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
#include "colour_conversion.h"
#include "font.h"
#include "dcpomatic_time.h"
#include "frame_rate_change.h"
#include "types.h"


class Content;
class Decoder;
struct check_reuse_old_data_test;


class Piece
{
public:
	Piece (boost::shared_ptr<Content> c, boost::shared_ptr<Decoder> d, FrameRateChange f);

	void update_pull_to (dcpomatic::DCPTime& pull_to) const;
	void set_last_push_end (AudioStreamPtr stream, dcpomatic::DCPTime last_push_end);
	boost::optional<dcpomatic::DCPTime> position (boost::shared_ptr<const Film> film);
	bool has_text () const;
	bool reference_audio () const;

	void pass ();
	void seek (boost::shared_ptr<const Film> film, dcpomatic::DCPTime time, bool accurate);

	dcpomatic::DCPTime position () const;
	dcpomatic::DCPTime end (boost::shared_ptr<const Film> film) const;
	FrameRateChange frame_rate_change () const {
		return _frc;
	}

	bool video_use () const;
	Crop video_crop () const;
	boost::optional<double> video_fade (boost::shared_ptr<const Film> film, Frame) const;
	boost::optional<ColourConversion> video_colour_conversion () const;
	VideoRange video_range () const;
	dcp::Size video_scaled_size (dcp::Size container_size) const;

	int audio_resampled_frame_rate (boost::shared_ptr<const Film> film) const;
	double audio_gain () const;

	dcpomatic::DCPTime content_video_to_dcp (Frame f) const;
	dcpomatic::DCPTime resampled_audio_to_dcp (boost::shared_ptr<const Film> film, Frame f) const;
	dcpomatic::ContentTime dcp_to_content_time (boost::shared_ptr<const Film> film, dcpomatic::DCPTime t) const;
	dcpomatic::DCPTime content_time_to_dcp (dcpomatic::ContentTime t) const;
	boost::optional<dcpomatic::DCPTime> content_time_to_dcp (boost::shared_ptr<Content> content, dcpomatic::ContentTime t) const;

	void add_fonts (std::list<boost::shared_ptr<dcpomatic::Font> >& fonts) const;

private:
	friend struct ::check_reuse_old_data_test;

	std::vector<boost::shared_ptr<Content> > _content;
	std::vector<boost::shared_ptr<Decoder> > _decoder;
	FrameRateChange _frc;
	bool _done;

	std::map<AudioStreamPtr, dcpomatic::DCPTime> _stream_last_push_end;
};

#endif

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


#ifndef DCPOMATIC_PIECE_H
#define DCPOMATIC_PIECE_H


#include "audio_stream.h"
#include "content_video.h"
#include "dcpomatic_time.h"
#include "frame_rate_change.h"
#include "types.h"
#include <map>


class Content;
class Decoder;
class PlayerVideo;
struct overlap_video_test1;


class Piece
{
public:
	Piece (std::shared_ptr<Content> c, std::shared_ptr<Decoder> d, FrameRateChange f);

	void update_pull_to (dcpomatic::DCPTime& pull_to) const;
	void set_last_push_end (AudioStreamPtr stream, dcpomatic::DCPTime last_push_end);

	dcpomatic::DCPTime content_video_to_dcp (Frame f) const;
	dcpomatic::DCPTime resampled_audio_to_dcp (Frame f, std::shared_ptr<const Film> film) const;
	dcpomatic::ContentTime dcp_to_content_time (dcpomatic::DCPTime t, std::shared_ptr<const Film> film) const;
	boost::optional<dcpomatic::DCPTime> content_time_to_dcp (std::shared_ptr<const Content> content, dcpomatic::ContentTime t) const;

	void pass ();

	bool use_video () const;
	VideoFrameType video_frame_type () const;

	FrameRateChange frame_rate_change () const {
		return frc;
	}

	dcpomatic::DCPTime position () const;
	dcpomatic::DCPTime end (std::shared_ptr<const Film> film) const;

	std::shared_ptr<PlayerVideo> player_video (ContentVideo video, std::shared_ptr<const Film> film, dcp::Size container_size) const;

	int resampled_audio_frame_rate (std::shared_ptr<const Film> film) const;
	double audio_gain () const;
	bool reference_dcp_audio () const;

	std::shared_ptr<Decoder> decoder_for (std::shared_ptr<Content> content) const;

	dcpomatic::DCPTime decoder_position () const;

	std::shared_ptr<Decoder> decoder;
	boost::optional<dcpomatic::DCPTimePeriod> ignore_video;
	FrameRateChange frc;
	bool done = false;

private:
	friend struct overlap_video_test1;

	std::shared_ptr<Content> _content;
	std::map<AudioStreamPtr, dcpomatic::DCPTime> _stream_last_push_end;
};


#endif

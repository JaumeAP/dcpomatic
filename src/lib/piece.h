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
#include "dcpomatic_font.h"
#include "dcpomatic_time.h"
#include "frame_rate_change.h"
#include "types.h"

class Content;
class Decoder;

class Piece
{
public:
	Piece (boost::shared_ptr<Content> c, boost::shared_ptr<Decoder> d, FrameRateChange f);

	void update_pull_to (dcpomatic::DCPTime& pull_to) const;
	void set_last_push_end (AudioStreamPtr stream, dcpomatic::DCPTime last_push_end);
	boost::optional<dcpomatic::DCPTime> position (boost::shared_ptr<const Film> film);
	bool has_text () const;

	dcpomatic::DCPTime position () const;
	dcpomatic::DCPTime end (boost::shared_ptr<const Film> film) const;

	dcpomatic::DCPTime content_video_to_dcp (Frame f) const;
	dcpomatic::DCPTime resampled_audio_to_dcp (boost::shared_ptr<const Film> film, Frame f) const;
	dcpomatic::ContentTime dcp_to_content_time (boost::shared_ptr<const Film> film, dcpomatic::DCPTime t) const;
	dcpomatic::DCPTime content_time_to_dcp (dcpomatic::ContentTime t) const;

	void add_fonts (std::list<boost::shared_ptr<dcpomatic::Font> >& fonts) const;

private:
	std::vector<boost::shared_ptr<Content> > _content;
	std::vector<boost::shared_ptr<Decoder> > _decoder;
	FrameRateChange _frc;
	bool _done;

	std::map<AudioStreamPtr, dcpomatic::DCPTime> _stream_last_push_end;
};

#endif

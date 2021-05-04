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
#include "dcpomatic_time.h"
#include "font_data.h"
#include "frame_rate_change.h"
#include "piece_atmos.h"
#include "piece_audio.h"
#include "piece_text.h"
#include "piece_video.h"
#include "types.h"
#include <boost/signals2.hpp>
#include <map>


class Content;
class Decoder;
class PlayerVideo;
class Resampler;
struct check_reuse_old_data_test;
struct overlap_video_test1;
struct player_time_calculation_test2;


class Piece
{
public:
	Piece (std::weak_ptr<const Film> film, std::shared_ptr<Content> content, std::shared_ptr<Decoder> decoder, FrameRateChange frc, bool fast);

	void update_pull_to (dcpomatic::DCPTime& pull_to) const;
	void set_last_push_end (int stream, dcpomatic::DCPTime last_push_end);

	boost::optional<dcpomatic::DCPTime> content_time_to_dcp (std::shared_ptr<const Content> content, dcpomatic::ContentTime t) const;

	void pass ();

	bool use_video () const;
	VideoFrameType video_frame_type () const;

	FrameRateChange frame_rate_change () const {
		return _frc;
	}

	dcpomatic::DCPTime position () const;
	dcpomatic::DCPTime end () const;
	dcpomatic::DCPTimePeriod period () const;

	std::shared_ptr<PlayerVideo> player_video (PieceVideo video, dcp::Size container_size) const;

	int resampled_audio_frame_rate () const;
	double audio_gain () const;
	bool reference_dcp_audio () const;

	std::shared_ptr<Decoder> decoder_for (std::shared_ptr<Content> content) const;

	void seek (dcpomatic::DCPTime time, bool accurate);
	boost::optional<dcpomatic::DCPTime> decoder_before(boost::optional<dcpomatic::DCPTime> time);
	std::vector<dcpomatic::FontData> fonts () const;

	void set_ignore_video (boost::optional<dcpomatic::DCPTimePeriod> period) {
		_ignore_video = period;
	}

	boost::signals2::signal<void (PieceVideo)> Video;
	boost::signals2::signal<void (PieceAudio)> Audio;
	boost::signals2::signal<void (PieceBitmapTextStart)> BitmapTextStart;
	boost::signals2::signal<void (PieceStringTextStart)> StringTextStart;
	boost::signals2::signal<void (PieceTextStop)> TextStop;
	boost::signals2::signal<void (PieceAtmos)> Atmos;

private:
	friend struct overlap_video_test1;
	friend struct check_reuse_old_data_test;
	friend struct player_time_calculation_test2;

	void video (std::shared_ptr<const ImageProxy> image, Frame frame, Eyes eyes, Part part);
	void audio (std::shared_ptr<AudioStream> stream, std::shared_ptr<const AudioBuffers> audio, Frame frame);
	void bitmap_start (std::weak_ptr<const Content> content, std::weak_ptr<const TextContent> text, dcpomatic::ContentTime time, std::shared_ptr<Image> image, dcpomatic::Rect<double> area);
	void string_start (std::weak_ptr<const Content> content, std::weak_ptr<const TextContent> text, dcpomatic::ContentTime time, std::list<dcp::SubtitleString> subs);
	void stop (std::weak_ptr<const Content> content, std::weak_ptr<const TextContent> text, dcpomatic::ContentTime time);
	void atmos (std::shared_ptr<const dcp::AtmosFrame> data, Frame frame, AtmosMetadata metadata);

	void flush ();
	bool done () const;

	dcpomatic::DCPTime content_video_to_dcp (Frame f) const;
	dcpomatic::ContentTime dcp_to_content_time (dcpomatic::DCPTime t) const;
	dcpomatic::DCPTime resampled_audio_to_dcp (Frame f) const;

	std::weak_ptr<const Film> _film;
	std::shared_ptr<Content> _content;
	std::shared_ptr<Decoder> _decoder;
	FrameRateChange _frc;
	bool _fast = false;
	boost::optional<dcpomatic::DCPTimePeriod> _ignore_video;

	struct Stream
	{
		Stream (dcpomatic::DCPTime lpe, AudioMapping m)
			: last_push_end(lpe)
			, mapping(m)
		{}

		dcpomatic::DCPTime last_push_end;
		std::shared_ptr<Resampler> resampler;
		Frame position = 0;
		AudioMapping mapping;
	};

	std::vector<Stream> _audio_streams;
};


#endif

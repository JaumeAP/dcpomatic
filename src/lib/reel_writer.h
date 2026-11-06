/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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


#include "atmos_metadata.h"
#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include "enum_indexed_vector.h"
#include "font_id_map.h"
#include "player_text.h"
#include "referenced_reel_asset.h"
#include "render_text.h"
#include "text_type.h"
#include "weak_film.h"
#include <dcp/atmos_asset_writer.h>
#include <dcp/file.h>
#include <dcp/ffmpeg_image.h>
#include <dcp/j2k_picture_asset_writer.h>
#include <dcp/mono_mpeg2_picture_frame.h>
#include <dcp/mpeg2_picture_asset_writer.h>


class AudioBuffers;
class Film;
class InfoFileHandle;
class Job;
class VideoMXFContent;
struct write_frame_info_test;

namespace dcp {
	class AtmosAsset;
	class MonoJ2KPictureAsset;
	class MonoJ2KPictureAssetWriter;
	class J2KPictureAsset;
	class J2KPictureAssetWriter;
	class MPEG2PictureAsset;
	class Reel;
	class ReelAsset;
	class ReelPictureAsset;
	class SoundAsset;
	class SoundAssetWriter;
	class StereoJ2KPictureAsset;
	class StereoJ2KPictureAssetWriter;
	class TextAsset;
}


/** A sound asset that a reel writes and other reels of the DCP refer to, and where in it they start and how much they play.
 *  It is shared between those reels' writers because the entry point and the duration are known only after the writer is
 *  made (when the head and tail of the sound are known).
 */
struct SharedSound
{
	std::shared_ptr<dcp::SoundAsset> asset;
	Frame entry_point = 0;
	boost::optional<Frame> duration;
};


class ReelWriter : public WeakConstFilm
{
public:
	ReelWriter(
		std::weak_ptr<const Film> film,
		dcpomatic::DCPTimePeriod period,
		std::shared_ptr<Job> job,
		int reel_index,
		int reel_count,
		bool text_only,
		boost::filesystem::path output_dir,
		std::shared_ptr<dcp::J2KPictureAsset> shared_picture = {},
		std::shared_ptr<SharedSound> shared_sound = {}
		);

	void write(std::shared_ptr<const dcp::Data> encoded, Frame frame, Eyes eyes);
	void fake_write(Frame frame, Eyes eyes);
	void repeat_write(Frame frame, Eyes eyes);
	void write(std::shared_ptr<const AudioBuffers> audio);
	void write(PlayerText text, TextType type, boost::optional<DCPTextTrack> track, dcpomatic::DCPTimePeriod period, FontIdMap const& fonts, std::shared_ptr<dcpomatic::Font> chosen_interop_font);
	void write(std::shared_ptr<const dcp::AtmosFrame> atmos, AtmosMetadata metadata);
	void write(std::shared_ptr<dcp::MonoMPEG2PictureFrame> image);

	/** Write @param head (which is @param head_frames video frames long) before the reel's sound and @param tail after
	 *  it, and have the CPL play only the reel's part.  Either may be null.
	 */
	void set_sound_ends(std::shared_ptr<const AudioBuffers> head, Frame head_frames, std::shared_ptr<const AudioBuffers> tail);

	/** Write the frames of a picture MXF that its start trim cuts off, if it starts this reel; call before any other picture */
	void write_picture_head();
	/** Write the frames of a picture MXF that its end trim cuts off, if it ends this reel; call after all other picture */
	void write_picture_tail();

	/** The sound asset that this reel writes, for other reels to refer to, or null if it writes none */
	std::shared_ptr<SharedSound> own_sound() const {
		return _own_sound;
	}

	/** The J2K picture asset of this reel, or null if it has none */
	std::shared_ptr<dcp::J2KPictureAsset> picture_asset() const {
		return _j2k_picture_asset;
	}

	void finish(boost::filesystem::path output_dcp);
	std::shared_ptr<dcp::Reel> create_reel(
		std::list<ReferencedReelAsset> const & refs,
		boost::filesystem::path output_dcp,
		bool ensure_subtitles,
		std::set<DCPTextTrack> ensure_closed_captions
		);
	void calculate_digests(std::function<void (int64_t, int64_t)> set_progress);

	Frame start() const;

	dcpomatic::DCPTimePeriod period() const {
		return _period;
	}

	int first_nonexistent_frame() const {
		return _first_nonexistent_frame;
	}

private:

	friend struct ::write_frame_info_test;

	Frame check_existing_picture_asset(boost::filesystem::path asset);
	bool existing_picture_frame_ok(dcp::File& asset_file, Frame frame);
	std::shared_ptr<dcp::TextAsset> empty_text_asset(TextType type, boost::optional<DCPTextTrack> track, bool with_dummy) const;

	std::shared_ptr<dcp::ReelPictureAsset> create_reel_picture(std::shared_ptr<dcp::Reel> reel, std::list<ReferencedReelAsset> const & refs) const;
	void create_reel_sound(std::shared_ptr<dcp::Reel> reel, std::list<ReferencedReelAsset> const & refs) const;
	void create_reel_text(
		std::shared_ptr<dcp::Reel> reel,
		std::list<ReferencedReelAsset> const & refs,
		int64_t duration,
		boost::filesystem::path output_dcp,
		bool ensure_subtitles,
		std::set<DCPTextTrack> ensure_closed_captions
		) const;
	void create_reel_markers(std::shared_ptr<dcp::Reel> reel) const;
	float convert_vertical_position(StringText const& subtitle, dcp::SubtitleStandard to) const;
	boost::filesystem::path reuse_existing_asset(DCPAsset const& existing_asset, std::shared_ptr<Job> job);
	void setup_video(std::vector<DCPAsset> const& reusable_assets, std::shared_ptr<Job> job, dcpomatic::DCPTimePeriod period, dcp::Standard standard);
	void setup_audio(std::vector<DCPAsset> const& reusable_assets, std::shared_ptr<Job> job, dcpomatic::DCPTimePeriod period, dcp::Standard standard);
	void setup_atmos(std::vector<DCPAsset> const& reusable_assets, std::shared_ptr<Job> job, dcpomatic::DCPTimePeriod period);

	boost::filesystem::path _output_dir;
	dcpomatic::DCPTimePeriod _period;
	/** the first picture frame index that does not already exist in our MXF */
	int _first_nonexistent_frame = 0;
	/** the data of the last written frame, if there is one */
	EnumIndexedVector<std::shared_ptr<const dcp::Data>, Eyes> _last_written;
	/** index of this reel within the DCP (starting from 0) */
	int _reel_index;
	/** number of reels in the DCP */
	int _reel_count;
	boost::optional<std::string> _content_summary;
	std::weak_ptr<Job> _job;
	bool _text_only;

	dcp::ArrayData _default_font;

	dcp::File _info_file;

	std::shared_ptr<dcp::J2KPictureAsset> _j2k_picture_asset;
	std::shared_ptr<dcp::MPEG2PictureAsset> _mpeg2_picture_asset;
	/** picture asset writer, or 0 if we are not writing any picture because we already have one */
	std::shared_ptr<dcp::J2KPictureAssetWriter> _j2k_picture_asset_writer;
	std::shared_ptr<dcp::MPEG2PictureAssetWriter> _mpeg2_picture_asset_writer;
	/** a picture asset written by another reel of this DCP, which this reel refers to, or null */
	std::shared_ptr<dcp::J2KPictureAsset> _shared_picture;
	/** entry point and duration to use in the CPL for a picture asset that we re-used, if they are not 0 and the asset's length */
	Frame _picture_entry_point = 0;
	boost::optional<Frame> _picture_duration;
	/** the picture MXF whose start trim this reel's picture asset keeps as a head, or null */
	std::shared_ptr<const VideoMXFContent> _picture_head_content;
	/** the picture MXF whose end trim this reel's picture asset keeps as a tail, or null */
	std::shared_ptr<const VideoMXFContent> _picture_tail_content;
	/** true if the picture asset has frames beyond this reel, so the CPL must say which of them to play */
	bool _picture_has_ends = false;
	/** frames of the picture asset before the frame that starts this reel */
	Frame _picture_head_frames = 0;
	std::shared_ptr<dcp::SoundAsset> _sound_asset;
	std::shared_ptr<dcp::SoundAssetWriter> _sound_asset_writer;
	/** entry point and duration to use in the CPL for a sound asset that we re-used, if they are not 0 and the asset's length */
	Frame _sound_entry_point = 0;
	boost::optional<Frame> _sound_duration;
	/** sound to write before the reel's own sound (the head of a sound MXF before its trim), if any */
	std::shared_ptr<const AudioBuffers> _sound_head;
	/** sound to write after the reel's own sound (the tail of a sound MXF after its trim), if any */
	std::shared_ptr<const AudioBuffers> _sound_tail;
	/** true once some of the reel's own sound has been written */
	bool _sound_written = false;
	/** our sound asset, as other reels refer to it, or null */
	std::shared_ptr<SharedSound> _own_sound;
	/** a sound asset written by another reel of this DCP, which this reel refers to, or null */
	std::shared_ptr<SharedSound> _shared_sound;
	std::shared_ptr<dcp::TextAsset> _subtitle_asset;
	std::map<DCPTextTrack, std::shared_ptr<dcp::TextAsset>> _closed_caption_assets;
	std::shared_ptr<dcp::AtmosAsset> _atmos_asset;
	/** entry point and duration to use in the CPL for an Atmos asset that we re-used, if they are not 0 and the asset's length */
	Frame _atmos_entry_point = 0;
	boost::optional<Frame> _atmos_duration;
	std::shared_ptr<dcp::AtmosAssetWriter> _atmos_asset_writer;

	mutable FontMetrics _font_metrics;
};

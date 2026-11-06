/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "crop.h"
#include "film.h"
#include "frame_rate_change.h"
#include "job.h"
#include "overlaps.h"
#include "video_content.h"
#include "video_mxf_content.h"
#include "video_mxf_examiner.h"
#include <asdcp/KM_log.h>
#include <dcp/exceptions.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/stereo_j2k_picture_asset.h>
#include <libxml++/libxml++.h>

#include "i18n.h"


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


VideoMXFContent::VideoMXFContent(boost::filesystem::path path)
	: Content(path)
{

}


VideoMXFContent::VideoMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version)
	: Content(node, film_directory)
{
	video = VideoContent::from_xml(this, node, version, VideoRange::FULL);
}


bool
VideoMXFContent::valid_mxf(boost::filesystem::path path)
{
	Kumu::DefaultLogSink().UnsetFilterFlag(Kumu::LOG_ALLOW_ALL);

	try {
		dcp::MonoJ2KPictureAsset mp(path);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	try {
		Kumu::DefaultLogSink().SetFilterFlag(0);
		dcp::StereoJ2KPictureAsset sp(path);
		return true;
	} catch (dcp::MXFFileError& e) {

	} catch (dcp::ReadError& e) {

	}

	Kumu::DefaultLogSink().SetFilterFlag(Kumu::LOG_ALLOW_ALL);

	return false;
}


void
VideoMXFContent::examine(shared_ptr<Job> job, bool tolerant)
{
	job->set_progress_unknown();

	Content::examine(job, tolerant);

	video.reset(new VideoContent(this));
	auto examiner = make_shared<VideoMXFExaminer>(shared_from_this());
	video->take_from_examiner(examiner);
	video->unset_colour_conversion();
}


string
VideoMXFContent::summary() const
{
	return fmt::format(_("{} [video]"), path_summary());
}


string
VideoMXFContent::technical_summary() const
{
	return Content::technical_summary() + " - " + video->technical_summary();
}


string
VideoMXFContent::identifier() const
{
	return Content::identifier() + "_" + video->identifier();
}


void
VideoMXFContent::as_xml(xmlpp::Element* element, bool with_paths, PathBehaviour path_behaviour, optional<boost::filesystem::path> film_directory) const
{
	cxml::add_text_child(element, "Type", "VideoMXF");
	Content::as_xml(element, with_paths, path_behaviour, film_directory);
	video->as_xml(element);
}


DCPTime
VideoMXFContent::full_length(shared_ptr<const Film> film) const
{
	FrameRateChange const frc(film, shared_from_this());
	return DCPTime::from_frames(llrint(video->length_after_3d_combine() * frc.factor()), film->video_frame_rate());
}


DCPTime
VideoMXFContent::approximate_length() const
{
	return DCPTime::from_frames(video->length_after_3d_combine(), 24);
}


void
VideoMXFContent::add_properties(shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties(film, p);
	video->add_properties(p);
}


vector<DCPAsset>
VideoMXFContent::assets(shared_ptr<const Film> film) const
{
	if (!video || !video->use()) {
		return {};
	}

	shared_ptr<dcp::MonoJ2KPictureAsset> asset;
	try {
		asset = make_shared<dcp::MonoJ2KPictureAsset>(path(0));
	} catch (dcp::MXFFileError&) {
		/* It is stereo */
		return {};
	} catch (dcp::ReadError&) {
		return {};
	}

	/* An encrypted file has a key of its own, which the DCP's KDMs do not carry */
	if (asset->encrypted() || film->three_d() || film->video_encoding() != VideoEncoding::JPEG2000 || film->encrypt_picture()) {
		return {};
	}

	if ((asset->standard() == dcp::Standard::INTEROP) != film->interop()) {
		return {};
	}

	FrameRateChange const frc(film, shared_from_this());
	if (frc.change_speed() || frc.skip() || frc.repeat() != 1) {
		return {};
	}

	auto const rate = film->video_frame_rate();
	auto const frame_rate = static_cast<double>(rate);
	if (trim_start() != trim_start().round(frame_rate) || trim_end() != trim_end().round(frame_rate)) {
		return {};
	}

	if (film->frame_size() != video->size() || video->frame_type() != VideoFrameType::TWO_D) {
		return {};
	}

	if (!(video->actual_crop() == Crop()) || video->custom_ratio() || video->custom_size()) {
		return {};
	}

	if (video->fade_in() != 0 || video->fade_out() != 0 || video->colour_conversion()) {
		return {};
	}

	DCPTimePeriod const period(position(), end(film));

	auto part = [](shared_ptr<const Content> c) {
		return static_cast<bool>(c->video) && c->video->use();
	};

	auto const others = dcpomatic::overlaps(film, film->content(), part, period.from, period.to);
	if (others.size() != 1 || others.front().get() != this) {
		return {};
	}

	return {
		DCPAsset(
			DCPAsset::Type::VIDEO,
			path(0),
			period,
			trim_start().frames_round(frame_rate),
			period.duration().frames_round(rate),
			boost::filesystem::path(fmt::format("j2c_{}.mxf", asset->id()))
			)
	};
}

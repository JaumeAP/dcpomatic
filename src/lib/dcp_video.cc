/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/dcp_video_frame.cc
 *  @brief A single frame of video destined for a DCP.
 *
 *  Given an Image and some settings, this class knows how to encode
 *  the image to J2K either on the local host or on a remote server.
 *
 *  Objects of this class are used for the queue that we keep
 *  of images that require encoding.
 */

#include "dcp_video.h"
#include "config.h"
#include "exceptions.h"
#include "encode_server_description.h"
#include "image.h"
#include "log.h"
#include "dcpomatic_log.h"
#include "cross.h"
#include "player_video.h"
#include "compose.hpp"
#include "warnings.h"
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include <dcp/openjpeg_image.h>
#include <dcp/rgb_xyz.h>
#include <dcp/j2k.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/thread.hpp>
#include <stdint.h>
#include <iomanip>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using dcp::Size;
using dcp::Data;
using dcp::raw_convert;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


#define DCI_COEFFICENT (48.0 / 52.37)

/** Construct a DCP video frame.
 *  @param frame Input frame.
 *  @param index Index of the frame within the DCP.
 *  @param bw J2K bandwidth to use (see Config::j2k_bandwidth ())
 */
DCPVideo::DCPVideo (
	shared_ptr<const PlayerVideo> frame, int index, int dcp_fps, int bw, Resolution r
	)
	: _frame (frame)
	, _index (index)
	, _frames_per_second (dcp_fps)
	, _j2k_bandwidth (bw)
	, _resolution (r)
{

}

DCPVideo::DCPVideo (shared_ptr<const PlayerVideo> frame, shared_ptr<const cxml::Node> node)
	: _frame (frame)
{
	_index = node->number_child<int> ("Index");
	_frames_per_second = node->number_child<int> ("FramesPerSecond");
	_j2k_bandwidth = node->number_child<int> ("J2KBandwidth");
	_resolution = Resolution (node->optional_number_child<int>("Resolution").get_value_or (RESOLUTION_2K));
}

shared_ptr<dcp::OpenJPEGImage>
DCPVideo::convert_to_xyz (shared_ptr<const PlayerVideo> frame, dcp::NoteHandler note)
{
	shared_ptr<dcp::OpenJPEGImage> xyz;

	shared_ptr<Image> image = frame->image (bind (&PlayerVideo::keep_xyz_or_rgb, _1), true, false);
	if (frame->colour_conversion()) {
		xyz = dcp::rgb_to_xyz (
			image->data()[0],
			image->size(),
			image->stride()[0],
			frame->colour_conversion().get(),
			note
			);
	} else {
		xyz.reset (new dcp::OpenJPEGImage (image->data()[0], image->size(), image->stride()[0]));
	}

	return xyz;
}


void
DCPVideo::add_metadata (xmlpp::Element* el) const
{
	el->add_child("Index")->add_child_text (raw_convert<string> (_index));
	el->add_child("FramesPerSecond")->add_child_text (raw_convert<string> (_frames_per_second));
	el->add_child("J2KBandwidth")->add_child_text (raw_convert<string> (_j2k_bandwidth));
	el->add_child("Resolution")->add_child_text (raw_convert<string> (int (_resolution)));
	_frame->add_metadata (el);
}


/** @return true if this DCPVideo is definitely the same as another;
 *  (apart from the frame index), false if it is probably not.
 */
bool
DCPVideo::same (shared_ptr<const DCPVideo> other) const
{
	if (_frames_per_second != other->_frames_per_second ||
	    _j2k_bandwidth != other->_j2k_bandwidth ||
	    _resolution != other->_resolution) {
		return false;
	}

	return _frame->same (other->_frame);
}

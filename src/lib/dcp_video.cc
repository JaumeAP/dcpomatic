/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "cross.h"
#include "dcp_video.h"
#include "player_video.h"
#include <libcxml/cxml.h>

#include "i18n.h"


using std::cout;
using std::make_shared;
using std::shared_ptr;
using std::string;


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
	_resolution = Resolution (node->optional_number_child<int>("Resolution").get_value_or(static_cast<int>(Resolution::TWO_K)));
}


Eyes
DCPVideo::eyes () const
{
	return _frame->eyes ();
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

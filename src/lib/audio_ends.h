/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#pragma once


#include "dcpomatic_time.h"
#include "types.h"
#include <memory>
#include <vector>


class AudioBuffers;
class Content;
class Film;


/** The sound that the trims of a content cut off at its start and end, as the film would write it */
struct AudioEnds
{
	/** the sound just before the start trim, or null */
	std::shared_ptr<const AudioBuffers> head;
	/** the sound just after the end trim, or null */
	std::shared_ptr<const AudioBuffers> tail;
	/** the length of the head in video frames */
	Frame head_frames = 0;
};


/** @param film Film that the content is in.
 *  @param group Contents with sound and no picture (WAVs, say) that play together: they have the same position, length and
 *  trims.  A sound MXF or a DCP is never one of them.
 *  @param want_head true to read the head.
 *  @param want_tail true to read the tail.
 *  @param reel_end the end of the reel that the tail goes with.
 *  @return the sound that the group's trims cut off, mixed, read from a second Player on copies of the contents with no
 *  trims, so that it has the changes the film makes (mapping, gain, resampling).  Each end is at most 10 seconds.  Nothing
 *  is returned if a delay, a speed change or a trim that is not a whole number of frames would put the ends out of step
 *  with the sound inside the trims, or if the contents do not play together.
 */
AudioEnds audio_ends(
	std::shared_ptr<const Film> film,
	std::vector<std::shared_ptr<const Content>> group,
	bool want_head,
	bool want_tail,
	dcpomatic::DCPTime reel_end
	);

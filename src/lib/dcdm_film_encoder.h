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


#ifndef DCPOMATIC_DCDM_FILM_ENCODER_H
#define DCPOMATIC_DCDM_FILM_ENCODER_H


#include "audio_mapping.h"
#include "butler.h"
#include "dcdm_audio_layout.h"
#include "event_history.h"
#include "film_encoder.h"
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>


/** @class DCDMFilmEncoder
 *  @brief Export a film as a DCDM: 12-bit XYZ DPX frames and 24-bit mono WAVs.
 *
 *  Picture is converted to XYZ exactly as for a DCP (DCPVideo::convert_to_xyz)
 *  and audio is what the player gives a DCP (gain, delay, trim, mapping and the
 *  audio processor applied), so that importing the result gives the same DCP.
 *
 *  Layout, per reel (in reel_NN/ when the film has more than one reel):
 *  frame_NNNNNNNN.dpx (or left/ and right/ for 3D) and audio/<label>.wav.
 */
class DCDMFilmEncoder : public FilmEncoder
{
public:
	/** @throw std::runtime_error if the film's sources are not good enough for a DCDM,
	 *  or it uses audio channels a DCDM cannot carry.
	 */
	DCDMFilmEncoder(std::shared_ptr<const Film> film, std::weak_ptr<Job> job, boost::filesystem::path output);

	/** @throw std::runtime_error if the output directory exists and is not empty */
	void go() override;

	boost::optional<float> current_rate() const override;
	Frame frames_done() const override;
	bool finishing() const override {
		return false;
	}

private:
	boost::filesystem::path reel_directory(int reel) const;

	boost::filesystem::path _output;
	DCDMLayout _layout;
	int _reels;

	mutable boost::mutex _mutex;
	dcpomatic::DCPTime _last_time;
	EventHistory _history;

	Butler _butler;
};


#endif

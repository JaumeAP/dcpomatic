/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_PIECE_AUDIO_H
#define DCPOMATIC_PIECE_AUDIO_H


/** @file  src/lib/piece_audio.h
 *  @brief PieceAudio class.
 */


#include "audio_buffers.h"
#include "audio_mapping.h"
#include "types.h"


/** @class PieceAudio
 *  @brief A block of audio from a Piece, with a timestamp as a frame within that piece.
 */
class PieceAudio
{
public:
	PieceAudio ()
		: audio (new AudioBuffers(0, 0))
	{}

	PieceAudio (int s, std::shared_ptr<const AudioBuffers> a, Frame f, dcpomatic::DCPTime t, AudioMapping m)
		: stream (s)
		, audio (a)
		, frame (f)
		, time (t)
		, mapping (m)
	{}

	int stream; ///< index of stream within the Piece
	std::shared_ptr<const AudioBuffers> audio;
	Frame frame = 0;
	dcpomatic::DCPTime time;
	AudioMapping mapping;
};


#endif

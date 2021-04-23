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


#include "audio_content.h"
#include "content.h"
#include "piece.h"


using std::shared_ptr;
using namespace dcpomatic;


Piece::Piece (shared_ptr<Content> c, shared_ptr<Decoder> d, FrameRateChange f)
	: content (c)
	, decoder (d)
	, frc (f)
{
	if (content->audio) {
		for (auto j: content->audio->streams()) {
			_stream_last_push_end[j] = content->position();
		}
	}
}



void
Piece::update_pull_to (DCPTime& pull_to) const
{
	if (done) {
		return;
	}

	for (auto const& i: _stream_last_push_end) {
		pull_to = std::min(pull_to, i.second);
	}
}


void
Piece::set_last_push_end (AudioStreamPtr stream, DCPTime end)
{
	DCPOMATIC_ASSERT (_stream_last_push_end.find(stream) != _stream_last_push_end.end());
	_stream_last_push_end[stream] = end;
}


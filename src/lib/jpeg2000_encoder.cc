/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "jpeg2000_encoder.h"
#include "poznan_encoder.h"
#include "openjpeg_encoder.h"
#include "exceptions.h"
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <vector>

using std::vector;
using std::string;
using boost::shared_ptr;

#define LOG_GENERAL(...) log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);

vector<boost::shared_ptr<JPEG2000Encoder> > JPEG2000Encoder::_encoders;

void
JPEG2000Encoder::setup_encoders ()
{
	try {
		_encoders.push_back (shared_ptr<JPEG2000Encoder> (new OpenJPEGEncoder ()));
	} catch (JPEG2000EncoderUnavailableException &) {
		
	}

	try {
		_encoders.push_back (shared_ptr<JPEG2000Encoder> (new PoznanEncoder ()));
	} catch (JPEG2000EncoderUnavailableException &) {

	}
}

vector<shared_ptr<JPEG2000Encoder> >
JPEG2000Encoder::all ()
{
	return _encoders;
}

shared_ptr<JPEG2000Encoder>
JPEG2000Encoder::from_id (string id)
{
	BOOST_FOREACH (shared_ptr<JPEG2000Encoder> i, _encoders) {
		if (i->id() == id) {
			return i;
		}
	}

	return shared_ptr<JPEG2000Encoder> ();
}

shared_ptr<EncodedData>
JPEG2000Encoder::encode (shared_ptr<const dcp::XYZImage> input,	dcp::NoteHandler note, int bandwidth, int frame_rate, Resolution resolution, bool threed)
{
	if (!_last_bandwidth || _last_bandwidth.get() != bandwidth) {
		_last_bandwidth = bandwidth;
		set_bandwidth (bandwidth);
	}

	if (!_last_frame_rate || _last_frame_rate.get() != frame_rate) {
		_last_frame_rate = frame_rate;
		set_frame_rate (frame_rate);
	}

	if (!_last_resolution || _last_resolution.get() != resolution) {
		_last_resolution = resolution;
		set_frame_rate (resolution);
	}
	
	if (!_last_threed || _last_threed.get() != threed) {
		_last_threed = threed;
		set_frame_rate (threed);
	}

	return do_encode (input, note);
}

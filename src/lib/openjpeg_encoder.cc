/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>
    Adapted from code Copyright (C) 2010-2011 Terrence Meiczinger

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

#include "openjpeg_encoder.h"
#include "exceptions.h"
#include <dcp/data.h>
#include <dcp/j2k.h>
#include <dcp/openjpeg_image.h>

#include "i18n.h"

using std::string;
using boost::shared_ptr;
using dcp::Data;

string
OpenJPEGEncoder::name () const
{
	return _("OpenJPEG library");
}

Data
OpenJPEGEncoder::do_encode (shared_ptr<const dcp::OpenJPEGImage> input)
{
	return dcp::compress_j2k (input, _bandwidth.get(), _frame_rate.get(), _threed.get(), _resolution.get() == RESOLUTION_4K);
}


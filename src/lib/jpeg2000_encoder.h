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

#ifndef DCPOMATIC_JPEG2000_ENCODER_H
#define DCPOMATIC_JPEG2000_ENCODER_H

#include "types.h"
#include <dcp/types.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <list>

namespace dcp {
	class XYZImage;
}

class EncodedData;

class JPEG2000Encoder
{
public:
	virtual ~JPEG2000Encoder () {}
	
	/** @return Internationalised descriptive name for this encoder */
	virtual std::string name () const = 0;
	/** @return Non-internationalised ID for this encoder */
	virtual std::string id () const = 0;

	/** @param input XYZ input frame.
	 *  @param note_handler Handler for notes about the encode.
	 
	 *  @return Encoded JPEG2000 data.
	 */
	boost::shared_ptr<EncodedData> encode (
		boost::shared_ptr<const dcp::XYZImage> input,
		int bandwidth,
		int frame_rate,
		Resolution resolution,
		bool threed
		);

 	static void setup_encoders ();
	/** @return Return all available JPEG2000 encoders in order of preference (best first) */
	static std::vector<boost::shared_ptr<JPEG2000Encoder> > all ();
	static boost::shared_ptr<JPEG2000Encoder> from_id (std::string id);

protected:

	virtual boost::shared_ptr<EncodedData> do_encode (
		boost::shared_ptr<const dcp::XYZImage> input
		) = 0;

	virtual void parameters_changed () {}

	boost::optional<int> _bandwidth;
	boost::optional<int> _frame_rate;
	boost::optional<Resolution> _resolution;
	boost::optional<bool> _threed;

	static std::vector<boost::shared_ptr<JPEG2000Encoder> > _encoders;
};

#endif

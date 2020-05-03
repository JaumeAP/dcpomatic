/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SCREEN_KDM_H
#define DCPOMATIC_SCREEN_KDM_H

#ifdef DCPOMATIC_VARIANT_SWAROOP
#include "encrypted_ecinema_kdm.h"
#endif
#include "recipient_with_kdm.h"
#include <dcp/encrypted_kdm.h>
#include <dcp/name_format.h>
#include <boost/shared_ptr.hpp>

namespace dcpomatic {
	class Screen;
}

/** Simple class to collect a screen and an encrypted KDM */
class ScreenWithKDM : public RecipientWithKDM
{
public:
	ScreenWithKDM (boost::shared_ptr<dcpomatic::Screen> s)
		: screen (s)
	{}

	virtual ~ScreenWithKDM () {}

	virtual std::string kdm_id () const = 0;

	void add_name_values (dcp::NameFormat::Map& values);

	boost::shared_ptr<dcpomatic::Screen> screen;
};

class DCPScreenWithKDM : public ScreenWithKDM
{
public:
	DCPScreenWithKDM (boost::shared_ptr<dcpomatic::Screen> s, dcp::EncryptedKDM k)
		: ScreenWithKDM (s)
		, kdm (k)
	{}

	std::string kdm_as_xml () const {
		return kdm.as_xml ();
	}

	void kdm_as_xml_to_file (boost::filesystem::path out) const {
		return kdm.as_xml (out);
	}

	std::string kdm_id () const {
		return kdm.cpl_id ();
	}

	dcp::EncryptedKDM kdm;
};

#ifdef DCPOMATIC_VARIANT_SWAROOP
class ECinemaScreenWithKDM : public ScreenWithKDM
{
public:
	ECinemaScreenWithKDM (boost::shared_ptr<dcpomatic::Screen> s, EncryptedECinemaKDM k)
		: ScreenWithKDM (s)
		, kdm (k)
	{}

	std::string kdm_as_xml () const {
		return kdm.as_xml ();
	}

	void kdm_as_xml_to_file (boost::filesystem::path out) const {
		return kdm.as_xml (out);
	}

	std::string kdm_id () const {
		return kdm.id ();
	}

	EncryptedECinemaKDM kdm;
};
#endif

#endif

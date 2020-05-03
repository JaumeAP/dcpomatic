/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_DKDM_RECIPIENT_H
#define DCPOMATIC_DKDM_RECIPIENT_H

#include "kdm_recipient.h"
#include <dcp/certificate.h>
#include <libcxml/cxml.h>

class DKDMRecipient : public KDMRecipient
{
public:
	DKDMRecipient (
		std::string const& name_,
		std::string const& notes_,
		boost::optional<dcp::Certificate> recipient_,
		std::list<std::string> const& emails_,
		int utc_offset_hour_,
		int utc_offset_minute_)
		: KDMRecipient (name_, notes_, recipient_)
		, emails (emails_)
		, _utc_offset_hour (utc_offset_hour_)
		, _utc_offset_minute (utc_offset_minute_)
	{}

	explicit DKDMRecipient (cxml::ConstNodePtr node);

	void as_xml (xmlpp::Element *) const;

	int utc_offset_hour () const {
		return _utc_offset_hour;
	}

	int utc_offset_minute () const {
		return _utc_offset_minute;
	}

	void set_utc_offset_hour (int h);
	void set_utc_offset_minute (int m);

	std::list<std::string> emails;

private:
	/** Offset such that the equivalent time in UTC can be determined
	    by subtracting the offset from the local time.
	*/
	int _utc_offset_hour;
	/** Additional minutes to add to _utc_offset_hour if _utc_offset_hour is
	    positive, or to subtract if _utc_offset_hour is negative.
	*/
	int _utc_offset_minute;
};

#endif

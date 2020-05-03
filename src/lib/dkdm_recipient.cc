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

#include "dkdm_recipient.h"
#include "dcpomatic_assert.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

using std::string;
using dcp::raw_convert;

DKDMRecipient::DKDMRecipient (cxml::ConstNodePtr node)
	: KDMRecipient (node)
{
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children("Email")) {
		emails.push_back (i->content());
	}

	_utc_offset_hour = node->number_child<int>("UTCOffsetHour");
	_utc_offset_minute = node->number_child<int>("UTCOffsetMinute");
}


void
DKDMRecipient::as_xml (xmlpp::Element* parent) const
{
	KDMRecipient::as_xml (parent);

	BOOST_FOREACH (string i, emails) {
		parent->add_child("Email")->add_child_text (i);
	}

	parent->add_child("UTCOffsetHour")->add_child_text(raw_convert<string>(_utc_offset_hour));
	parent->add_child("UTCOffsetMinute")->add_child_text(raw_convert<string>(_utc_offset_minute));
}


void
DKDMRecipient::set_utc_offset_hour (int h)
{
	DCPOMATIC_ASSERT (h >= -11 && h <= 12);
	_utc_offset_hour = h;
}


void
DKDMRecipient::set_utc_offset_minute (int m)
{
	DCPOMATIC_ASSERT (m >= 0 && m <= 59);
	_utc_offset_minute = m;
}

/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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

#include "screen.h"
#include "cinema.h"
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

using std::string;
using std::vector;
using namespace dcpomatic;

Screen::Screen (cxml::ConstNodePtr node)
	: KDMRecipient (node)
{
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children ("TrustedDevice")) {
		if (boost::algorithm::starts_with(i->content(), "-----BEGIN CERTIFICATE-----")) {
			trusted_devices.push_back (TrustedDevice(dcp::Certificate(i->content())));
		} else {
			trusted_devices.push_back (TrustedDevice(i->content()));
		}
	}
}

void
Screen::as_xml (xmlpp::Element* parent) const
{
	KDMRecipient::as_xml (parent);
	BOOST_FOREACH (TrustedDevice i, trusted_devices) {
		parent->add_child("TrustedDevice")->add_child_text(i.as_string());
	}
}

vector<string>
Screen::trusted_device_thumbprints () const
{
	vector<string> t;
	BOOST_FOREACH (TrustedDevice i, trusted_devices) {
		t.push_back (i.thumbprint());
	}
	return t;
}


int
Screen::utc_offset_hour () const
{
	return cinema ? cinema->utc_offset_hour() : 0;
}


int
Screen::utc_offset_minute () const
{
	return cinema ? cinema->utc_offset_minute() : 0;
}


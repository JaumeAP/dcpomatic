/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "font.h"
#include "dcpomatic_assert.h"
#include "dcpomatic_socket.h"
#include <dcp/data.h>
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

using std::string;
using boost::shared_ptr;
using dcp::raw_convert;
using namespace dcpomatic;

Font::Font (cxml::NodePtr node)
	: _id (node->string_child ("Id"))
{
	BOOST_FOREACH (cxml::NodePtr i, node->node_children("File")) {
		string variant = i->optional_string_attribute("Variant").get_value_or ("Normal");
		if (variant == "Normal") {
			_file = i->content();
		}
	}
}

void
Font::as_xml (xmlpp::Node* node) const
{
	node->add_child("Id")->add_child_text (_id);
	if (_file) {
		node->add_child("File")->add_child_text(_file->string());
	}
}

/** Add things to an XML node to describe this font for transfer across
 *  a network to another machine.  The companion method send_binary() will be called
 *  to send binary parts.
 */
void
Font::transfer_xml (xmlpp::Node* node) const
{
	node->add_child("Id")->add_child_text (_id);
	if (_file) {
		node->add_child("FileLength")->add_child_text(raw_convert<string>(boost::filesystem::file_size(*_file)));
	}
}

void
Font::transfer_binary (shared_ptr<Socket> socket) const
{
	if (_file) {
		dcp::Data data (*_file);
		socket->write (data.data().get(), data.size());
	}
}

bool
dcpomatic::operator== (Font const & a, Font const & b)
{
	if (a.id() != b.id()) {
		return false;
	}

	return a.file() == b.file();
}

bool
dcpomatic::operator!= (Font const & a, Font const & b)
{
	return !(a == b);
}

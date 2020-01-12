/*
    Copyright (C) 2014-2019 Carl Hetherington <cth@carlh.net>

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

#include "player_text.h"
#include "font.h"
#include "util.h"
#include "dcpomatic_socket.h"
#include "image.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

using std::list;
using boost::shared_ptr;
using dcp::raw_convert;
using namespace dcpomatic;

void
PlayerText::add_metadata (xmlpp::Node* node) const
{
	BOOST_FOREACH (shared_ptr<Font> i, fonts) {
		/* XXX: transferring a font file for every frame that needs it seems a bit wasteful,
		   but probably not so bad in the great scheme of things.
		*/
		i->transfer_xml (node->add_child("Font"));
	}

	BOOST_FOREACH (BitmapText i, bitmap) {
		i.transfer_xml (node->add_child("Bitmap"));
	}

	BOOST_FOREACH (StringText i, string) {
		i.transfer_xml (node->add_child("String"));
	}
}

void
PlayerText::send_binary (shared_ptr<Socket> socket) const
{
	BOOST_FOREACH (shared_ptr<Font> i, fonts) {
		i->transfer_binary (socket);
	}

	BOOST_FOREACH (BitmapText i, bitmap) {
		i.transfer_binary (socket);
	}
}

void
PlayerText::add_fonts (list<shared_ptr<Font> > fonts_)
{
	BOOST_FOREACH (shared_ptr<Font> i, fonts_) {
		bool got = false;
		BOOST_FOREACH (shared_ptr<Font> j, fonts) {
			if (*i == *j) {
				got = true;
			}
		}
		if (!got) {
			fonts.push_back (i);
		}
	}
}

bool
operator== (PlayerText const & a, PlayerText const & b)
{
	return deep_equals(a.fonts, b.fonts) && deep_equals(a.bitmap, b.bitmap) && deep_equals(a.string, b.string);
}

bool
operator!= (PlayerText const & a, PlayerText const & b)
{
	return !(a == b);
}

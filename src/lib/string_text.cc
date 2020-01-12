/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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

#include "string_text.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>

using std::string;
using dcp::raw_convert;

bool
operator== (StringText const & a, StringText const & b)
{
	return static_cast<dcp::SubtitleString const &>(a) == static_cast<dcp::SubtitleString const &>(b) && a.outline_width == b.outline_width;
}

void
StringText::transfer_xml (xmlpp::Node* node) const
{
	if (font()) {
		node->add_child("Font")->add_child_text(*font());
	}
	node->add_child("Italic")->add_child_text(italic() ? "1" : "0");
	node->add_child("Bold")->add_child_text(bold() ? "1" : "0");
	node->add_child("Underline")->add_child_text(underline() ? "1" : "0");
	node->add_child("Colour")->add_child_text(colour().to_argb_string());
	node->add_child("Size")->add_child_text(raw_convert<string>(size()));
	node->add_child("AspectAdjust")->add_child_text(raw_convert<string>(aspect_adjust()));
	node->add_child("Direction")->add_child_text(dcp::direction_to_string(direction()));
	node->add_child("Text")->add_child_text(text());
	node->add_child("Effect")->add_child_text(dcp::effect_to_string(effect()));
	node->add_child("EffectColour")->add_child_text(effect_colour().to_argb_string());

	node->add_child("OutlineWidth")->add_child_text(raw_convert<string>(outline_width));
}



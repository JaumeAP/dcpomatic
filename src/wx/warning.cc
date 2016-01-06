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

#include "lib/cross.h"
#include "warning.h"
#include "wx_util.h"
#include <boost/filesystem.hpp>

using boost::optional;

optional<wxBitmap> Warning::_blank;
optional<wxBitmap> Warning::_warning;

Warning::Warning (wxWindow* parent)
{
	if (!_blank) {
		boost::filesystem::path p = shared_path() / "blank.png";
		_blank = wxBitmap ();
		_blank->LoadFile (std_to_wx(p.string()), wxBITMAP_TYPE_PNG);
	}

	if (!_warning) {
		boost::filesystem::path p = shared_path() / "warning.png";
		_warning = wxBitmap ();
		_warning->LoadFile (std_to_wx(p.string()), wxBITMAP_TYPE_PNG);
	}

	_bitmap = new wxStaticBitmap (parent, wxID_ANY, _blank.get());
}

void
Warning::enable (wxString tooltip)
{
	_bitmap->SetBitmap (_warning.get ());
	_bitmap->SetToolTip (tooltip);
}

void
Warning::disable ()
{
	_bitmap->SetBitmap (_blank.get ());
}

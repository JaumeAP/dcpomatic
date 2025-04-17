/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_meters_dialog.h"
#include <wx/wx.h>


AudioMetersDialog::AudioMetersDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Meters"))
{

}


void
AudioMetersDialog::set_meters(wxWindow* meters)
{
	meters->Reparent(this);

	auto sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(meters, 1, wxEXPAND);
	SetSizer(sizer);

	SetSize({640, 480});
	sizer->Layout();
}


/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "advanced_dcp_settings_dialog.h"
#include "check_box.h"
#include "wx_util.h"
#include "lib/film.h"


using std::shared_ptr;


AdvancedDCPSettingsDialog::AdvancedDCPSettingsDialog(wxWindow* parent, shared_ptr<const Film> film)
	: wxDialog(parent, wxID_ANY, _("Advanced DCP settings"))
{
	auto sizer = new wxBoxSizer(wxVERTICAL);

	_write_cpl_metadata = new CheckBox(this, _("Write CPL metadata"));
	_write_cpl_metadata->set(film->write_cpl_metadata());

	sizer->Add(_write_cpl_metadata, 0, wxALL, DCPOMATIC_SIZER_GAP);

	auto buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add(buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit(sizer);
}


bool
AdvancedDCPSettingsDialog::write_cpl_metadata() const
{
	return _write_cpl_metadata->get();
}


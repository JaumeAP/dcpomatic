/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "static_text.h"
#include "upload_destination_panel.h"
#include "upload_dialog.h"
#include "wx_util.h"
#include <wx/wx.h>


using std::vector;


UploadDialog::UploadDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Upload DCP"))
	, _upload_destination_panel(new UploadDestinationPanel(this))
{
	auto sizer = new wxBoxSizer(wxVERTICAL);

	wxFont subheading_font(*wxNORMAL_FONT);
	subheading_font.SetWeight(wxFONTWEIGHT_BOLD);

	sizer->Add(_upload_destination_panel, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto buttons = CreateStdDialogButtonSizer(0);
	sizer->Add(CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
	_start_upload = new wxButton(this, wxID_OK, _("Start upload"));
	buttons->SetAffirmativeButton(_start_upload);
	buttons->SetNegativeButton(new wxButton(this, wxID_CANCEL, _("Cancel")));
	buttons->Realize();

	_upload_destination_panel->DestinationsChanged.connect(boost::bind(&UploadDialog::setup_sensitivity, this));
	setup_sensitivity();

	SetSizer(sizer);
	SetSize({640, 480});
}


vector<UploadDestination>
UploadDialog::destinations() const
{
	return _upload_destination_panel->destinations();
}


void
UploadDialog::setup_sensitivity()
{
	_start_upload->Enable(!_upload_destination_panel->destinations().empty());
}


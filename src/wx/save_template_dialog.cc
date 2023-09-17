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


#include "check_box.h"
#include "dcpomatic_choice.h"
#include "save_template_dialog.h"
#include "wx_util.h"
#include "lib/config.h"


using std::string;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


SaveTemplateDialog::SaveTemplateDialog(wxWindow* parent, optional<string> last_template)
	: TableDialog (parent, _("Save template"), 2, 1, true)
{
	_as_existing = new wxRadioButton(this, wxID_ANY, "Save over existing template");
	add(_as_existing, true);
	_existing = new Choice(this);
	add(_existing);

	const auto existing_templates = Config::instance()->templates();
	for (auto i: existing_templates) {
		_existing->add(i);
	}

	_new = new wxRadioButton(this, wxID_ANY, _("Make new template with name"));
	add(_new, true);
	_name = add (new wxTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (300, -1)));
	_name->SetFocus ();
	layout ();

	_as_existing->Enable(!existing_templates.empty());
	_existing->Enable(!existing_templates.empty());

	if (last_template) {
		auto iter = std::find(existing_templates.begin(), existing_templates.end(), *last_template);
		if (iter != existing_templates.end()) {
			_as_existing->SetValue(true);
			_existing->set(std::distance(existing_templates.begin(), iter));
		}
	} else {
		_new->SetValue(true);
		_existing->set(0);
	}


	auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this));
	if (ok) {
		ok->Bind(wxEVT_BUTTON, boost::bind(&SaveTemplateDialog::check, this, _1));
	}

	_as_existing->Bind(wxEVT_RADIOBUTTON, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));
	_new->Bind(wxEVT_RADIOBUTTON, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));
	_name->Bind(wxEVT_TEXT, boost::bind(&SaveTemplateDialog::setup_sensitivity, this));

	setup_sensitivity ();
}


void
SaveTemplateDialog::setup_sensitivity ()
{
	_existing->Enable(_as_existing->GetValue());
	_name->Enable(_new->GetValue());
	if (_new->GetValue()) {
		_name->SetFocus();
	}
	if (auto ok = dynamic_cast<wxButton *>(FindWindowById(wxID_OK, this))) {
		ok->Enable(_as_existing->GetValue() || (_new->GetValue() && !_name->GetValue().IsEmpty()));
	}
}


string
SaveTemplateDialog::name () const
{
	if (_as_existing->GetValue()) {
		auto const templates = Config::instance()->templates();
		auto selection = _existing->get();
		DCPOMATIC_ASSERT(selection && *selection < int(templates.size()));
		return templates[*selection];
	} else {
		return wx_to_std(_name->GetValue());
	}
}


void
SaveTemplateDialog::check (wxCommandEvent& ev)
{
	bool ok = true;

	if (_new->GetValue() && Config::instance()->existing_template(wx_to_std(_name->GetValue()))) {
		ok = confirm_dialog (this, _("There is already a template with this name.  Do you want to overwrite it?"));
	}

	if (ok) {
		ev.Skip ();
	}
}

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


#include "dcpomatic_button.h"
#include "upload_destination_dialog.h"
#include "upload_destination_panel.h"
#include "wx_util.h"
#include "lib/config.h"
#include <wx/treelist.h>


using std::string;
using std::vector;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


UploadDestinationPanel::UploadDestinationPanel(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
{
	_list = new wxTreeListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTL_NO_HEADER | wxTL_3STATE);
	_list->AppendColumn(char_to_wx("foo"), 640);

	for (auto destination: Config::instance()->upload_destinations()) {
		add_destination(destination);
	}

	auto buttons = new wxBoxSizer(wxVERTICAL);

	_add = new Button(this, _("Add..."));
	buttons->Add(_add, 0, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_edit = new Button(this, _("Edit..."));
	buttons->Add(_edit, 0, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);
	_remove = new Button(this, _("Remove"));
	buttons->Add(_remove, 0, wxEXPAND | wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	_add->bind(&UploadDestinationPanel::add, this);
	_edit->bind(&UploadDestinationPanel::edit, this);
	_remove->bind(&UploadDestinationPanel::remove, this);

	_list->Bind(wxEVT_TREELIST_SELECTION_CHANGED, boost::bind(&UploadDestinationPanel::selection_changed, this));
	_list->Bind(wxEVT_TREELIST_ITEM_CHECKED, boost::bind(&UploadDestinationPanel::checkbox_changed, this));

	auto sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(_list, 1, wxEXPAND);
	sizer->Add(buttons, 0, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_X_GAP);
	SetSizerAndFit(sizer);
}


void
UploadDestinationPanel::set_selected(vector<string> const& selected)
{
	for (auto  // wip!
	_list->SetItemState();
}


void
UploadDestinationPanel::checkbox_changed()
{
	DestinationsChanged();
}


void
UploadDestinationPanel::selection_changed()
{
	setup_sensitivity();
}


void
UploadDestinationPanel::setup_sensitivity()
{
	auto const selection = static_cast<bool>(selected());
	_edit->Enable(selection);
	_remove->Enable(selection);
}


void
UploadDestinationPanel::add_destination(UploadDestination destination)
{
	auto const id = _list->AppendItem(_list->GetRootItem(), std_to_wx(destination.name));
	_destinations.push_back(std::make_pair(id, destination));
}


void
UploadDestinationPanel::add()
{
	UploadDestinationDialog dialog(this);
	if (dialog.ShowModal() == wxID_OK) {
		add_destination(dialog.get()[0]);
		update_config();
	}
}


void
UploadDestinationPanel::update_config()
{
	vector<UploadDestination> dest;
	for (auto const& i: _destinations) {
		dest.push_back(i.second);
	}
	Config::instance()->set_upload_destinations(dest);
}


optional<wxTreeListItem>
UploadDestinationPanel::selected() const
{
	wxTreeListItems selection;
	_list->GetSelections(selection);
	DCPOMATIC_ASSERT(selection.size() <= 1);
	if (selection.empty()) {
		return {};
	}

	return selection[0];
}

void
UploadDestinationPanel::edit()
{
	if (auto sel = selected()) {
		for (auto& i: _destinations) {
			if (i.first == *sel) {
				UploadDestinationDialog dialog(this);
				dialog.set(i.second);
				if (dialog.ShowModal() == wxID_OK) {
					i.second = dialog.get()[0];
					update_config();
					_list->SetItemText(*sel, std_to_wx(i.second.name));
				}
			}
		}
	}
}


void
UploadDestinationPanel::remove()
{
	if (auto sel = selected()) {
		for (auto iter = _destinations.begin(); iter != _destinations.end(); ) {
			if (iter->first == *sel) {
				iter = _destinations.erase(iter);
				_list->DeleteItem(iter->first);
			} else {
				++iter;
			}
		}
		update_config();
	}

}


vector<UploadDestination>
UploadDestinationPanel::destinations() const
{
	vector<UploadDestination> dest;

	for (auto child = _list->GetFirstChild(_list->GetRootItem()); child.IsOk(); child = _list->GetNextSibling(child)) {
		if (_list->GetCheckedState(child) == wxCHK_CHECKED) {
			for (auto i: _destinations) {
				if (i.first == child) {
					dest.push_back(i.second);
				}
			}
		}
	}

	return dest;
}


/*
    Copyright (C) 2023 Grok Image Compression Inc.

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


#ifndef DCPOMATIC_GROK_GPU_CONFIG_PANEL
#define DCPOMATIC_GROK_GPU_CONFIG_PANEL


#include "lib/grok/util.h"
#include <boost/bind/bind.hpp>
#include <wx/filepicker.h>
#include <wx/wx.h>


class GpuList : public wxPanel
{
public:
	GpuList(wxPanel* parent)
		: wxPanel(parent, wxID_ANY)
	{
		_combo_box = new wxComboBox(this, wxID_ANY, wxString{}, wxDefaultPosition, wxSize(400, -1));
		_combo_box->Bind(wxEVT_COMBOBOX, &GpuList::OnComboBox, this);
		update();

		auto sizer = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(_combo_box, 0, wxALIGN_CENTER_VERTICAL);
		SetSizerAndFit(sizer);
	}

	void update()
	{
		_combo_box->Clear();
		for (auto const& name: get_gpu_names()) {
			_combo_box->Append(std_to_wx(name));
		}
	}

	void set_selection(int sel)
	{
		if (sel < static_cast<int>(_combo_box->GetCount())) {
			_combo_box->SetSelection(sel);
		}
	}

private:
	void OnComboBox(wxCommandEvent&)
	{
		auto selection = _combo_box->GetSelection();
		if (selection != wxNOT_FOUND) {
			auto grok = Config::instance()->grok();
			grok.selected = selection;
			Config::instance()->set_grok(grok);
		}
	}

	wxComboBox* _combo_box;
	int _selection = 0;
};


class GrokConfigPanel : public wxPanel
{
public:
	GrokConfigPanel(wxWindow* parent)
		: wxPanel(parent, wxID_ANY)
	{
		auto sizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(sizer);

		auto table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
		table->AddGrowableCol(1, 1);
		sizer->Add(table, 1, wxEXPAND);

		add_label_to_sizer(table, this, _("Acceleration binary folder"), true, 0, wxLEFT | wxLEFT | wxALIGN_CENTRE_VERTICAL);
		_binary_location = new wxDirPickerCtrl(this, wxDD_DIR_MUST_EXIST);
		table->Add(_binary_location, 1, wxEXPAND);

		add_label_to_sizer(table, this, _("GPU selection"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_gpu_list_control = new GpuList(this);
		table->Add(_gpu_list_control, 1, wxEXPAND);

		add_label_to_sizer(table, this, _("License server"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_server = new wxTextCtrl(this, wxID_ANY);
		table->Add(_server, 1, wxEXPAND | wxALL);

		add_label_to_sizer(table, this, _("License"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
		_licence = new PasswordEntry(this);
		table->Add(_licence->get_panel(), 1, wxEXPAND | wxALL);

		_binary_location->Bind(wxEVT_DIRPICKER_CHANGED, boost::bind(&GrokConfigPanel::binary_location_changed, this));
		_server->Bind(wxEVT_TEXT, boost::bind(&GrokConfigPanel::server_changed, this));
		_licence->Changed.connect(boost::bind(&GrokConfigPanel::licence_changed, this));
	}

	void config_changed()
	{
		auto grok = Config::instance()->grok();

		_binary_location->SetPath(std_to_wx(grok.binary_location.string()));
		_gpu_list_control->update();
		_gpu_list_control->set_selection(grok.selected);
		checked_set(_server, grok.licence_server);
		checked_set(_licence, grok.licence);
	}

	void binary_location_changed()
	{
		auto grok = Config::instance()->grok();
		grok.binary_location = wx_to_std(_binary_location->GetPath());
		Config::instance()->set_grok(grok);

		_gpu_list_control->update();
	}

	void server_changed()
	{
		auto grok = Config::instance()->grok();
		grok.licence_server = wx_to_std(_server->GetValue());
		Config::instance()->set_grok(grok);
	}

	void port_changed()
	{
		auto grok = Config::instance()->grok();
		Config::instance()->set_grok(grok);
	}

	void licence_changed()
	{
		auto grok = Config::instance()->grok();
		grok.licence = _licence->get();
		Config::instance()->set_grok(grok);
	}

	CheckBox* _enable_gpu = nullptr;
	wxDirPickerCtrl* _binary_location = nullptr;
	GpuList* _gpu_list_control = nullptr;
	wxTextCtrl* _server = nullptr;
	PasswordEntry* _licence = nullptr;
};


#endif


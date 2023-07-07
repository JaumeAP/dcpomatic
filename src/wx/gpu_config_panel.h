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


#include "config_dialog.h"


#pragma once


class CheckBox;
class DirPickerCtrl;
class GPUList;
class wxTextCtrl;


class GPUPage : public Page
{
public:
	GPUPage(wxSize panel_size, int border);

	wxString GetName() const override;

#ifdef DCPOMATIC_OSX
	wxBitmap GetLargeIcon() const override;
#endif

private:
	void setup() override;
	void config_changed() override;
	void enable_gpu_changed();
	void binary_location_changed();
	void server_changed();
	void port_changed();
	void license_changed();

	CheckBox* _enable_gpu;
	DirPickerCtrl* _binary_location;
	GPUList* _gpu_list_control;
	wxTextCtrl* _server;
	wxSpinCtrl* _port;
	PasswordEntry* _license;
};

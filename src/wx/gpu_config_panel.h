#include "config_dialog.h"


#pragma once


class CheckBox;
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
	wxDirPickerCtrl* _binary_location;
	GPUList* _gpu_list_control;
	wxTextCtrl* _server;
	wxSpinCtrl* _port;
	PasswordEntry* _license;
};

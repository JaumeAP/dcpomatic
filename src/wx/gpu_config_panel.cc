
#include "check_box.h"
#include "gpu_config_panel.h"
#include "password_entry.h"
#include <wx/panel.h>
#include <wx/wx.h>

#include <fstream>
#include <string>
#include <vector>


static std::vector<std::string>
get_gpu_names(std::string binary, std::string filename)
{
	// Execute the GPU listing program and redirect its output to a file
	std::system((binary + " > " + filename).c_str());

	std::vector<std::string> gpu_names;
	std::ifstream file(filename);
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			gpu_names.push_back(line);
		}
		file.close();
	}

	return gpu_names;
}


class GPUList : public wxPanel
{
public:
	GPUList(wxPanel* parent)
		: wxPanel(parent, wxID_ANY)
		, _selection(0)
	{
		_combo_box = new wxComboBox(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, -1));
		_combo_box->Bind(wxEVT_COMBOBOX, &GPUList::on_combo_box, this);
		update();

		wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);

		sizer->Add(_combo_box, 0, wxALIGN_CENTER_VERTICAL); // Vertically center the comboBox

		this->SetSizerAndFit(sizer);
	}

	void update()
	{
		auto cfg = Config::instance();
		auto lister_binary = cfg->gpu_binary_location() + "/" + "gpu_lister";
		auto lister_file = cfg->gpu_binary_location() + "/" + "gpus.txt";
		if (boost::filesystem::exists(lister_binary)) {
			auto gpu_names = get_gpu_names(lister_binary, lister_file);

			_combo_box->Clear();
			for (const auto& name : gpu_names) {
				_combo_box->Append(name);
			}
		}
	}

	int get_selection()
	{
		return _selection;
	}

	void set_selection(int sel)
	{
		if ((int)_combo_box->GetCount() > sel) {
			_combo_box->SetSelection(sel);
		}
	}

private:
	void on_combo_box([[maybe_unused]] wxCommandEvent& event)
	{
		_selection = _combo_box->GetSelection();
		if (_selection != wxNOT_FOUND) {
			Config::instance()->set_selected_gpu(_selection);
		}
	}

	wxComboBox* _combo_box;
	int _selection;
};


GPUPage::GPUPage(wxSize panel_size, int border)
	: Page(panel_size, border)
	, _enable_gpu(nullptr)
	, _binary_location(nullptr)
	, _gpu_list_control(nullptr)
{

}


wxString
GPUPage::GetName() const
{
	return _("GPU");
}


#ifdef DCPOMATIC_OSX
wxBitmap
GPUPage::GetLargeIcon() const
{
	return wxBitmap(icon_path("tms"), wxBITMAP_TYPE_PNG);
}
#endif


void
GPUPage::setup()
{
	auto config = Config::instance();

	_enable_gpu = new CheckBox(_panel, _("Enable GPU acceleration"));
	_panel->GetSizer()->Add(_enable_gpu, 0, wxALL | wxEXPAND, _border);

	wxFlexGridSizer* table = new wxFlexGridSizer(2, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	table->AddGrowableCol(1, 1);
	_panel->GetSizer()->Add(table, 1, wxALL | wxEXPAND, _border);

	add_label_to_sizer(table, _panel, _("Acceleration binary folder"), true, 0, wxLEFT | wxLEFT | wxALIGN_CENTRE_VERTICAL);
	_binary_location = new wxDirPickerCtrl(_panel, wxDD_DIR_MUST_EXIST);
	table->Add(_binary_location, 1, wxEXPAND);

	add_label_to_sizer(table, _panel, _("GPU selection"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_gpu_list_control = new GPUList(_panel);
	table->Add(_gpu_list_control, 1, wxEXPAND);

	add_label_to_sizer(table, _panel, _("License server"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_server = new wxTextCtrl(_panel, wxID_ANY);
	table->Add(_server, 1, wxEXPAND | wxALL);

	add_label_to_sizer(table, _panel, _("Port"), false, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_port = new wxSpinCtrl(_panel, wxID_ANY);
	_port->SetRange(0, 65535);
	table->Add(_port);

	add_label_to_sizer(table, _panel, _("License"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	_license = new PasswordEntry(_panel);
	table->Add(_license->get_panel(), 1, wxEXPAND | wxALL);

	_enable_gpu->bind(&GPUPage::enable_gpu_changed, this);
	_binary_location->Bind(wxEVT_DIRPICKER_CHANGED, boost::bind(&GPUPage::binary_location_changed, this));
	_server->Bind(wxEVT_TEXT, boost::bind(&GPUPage::server_changed, this));
	_port->Bind(wxEVT_SPINCTRL, boost::bind(&GPUPage::port_changed, this));
	_license->Changed.connect(boost::bind(&GPUPage::license_changed, this));

	_binary_location->Enable(config->enable_gpu());
	_gpu_list_control->Enable(config->enable_gpu());
	_server->Enable(config->enable_gpu());
	_port->Enable(config->enable_gpu());
	_license->get_panel()->Enable(config->enable_gpu());
}


void GPUPage::config_changed()
{
	auto config = Config::instance();

	checked_set(_enable_gpu, config->enable_gpu());
	_binary_location->SetPath(config->gpu_binary_location());
	_gpu_list_control->update();
	_gpu_list_control->set_selection(config->selected_gpu());
	checked_set(_server, config->gpu_license_server());
	checked_set(_port, config->gpu_license_port());
	checked_set(_license, config->gpu_license());
}


void GPUPage::enable_gpu_changed()
{
	auto config = Config::instance();

	config->set_enable_gpu(_enable_gpu->GetValue());
	_binary_location->Enable(config->enable_gpu());
	_gpu_list_control->Enable(config->enable_gpu());
	_server->Enable(config->enable_gpu());
	_port->Enable(config->enable_gpu());
	_license->get_panel()->Enable(config->enable_gpu());
}


void GPUPage::binary_location_changed()
{
	Config::instance()->set_gpu_binary_location(wx_to_std(_binary_location->GetPath()));
	_gpu_list_control->update();
}


void GPUPage::server_changed()
{
	Config::instance()->set_gpu_license_server(wx_to_std(_server->GetValue()));
}


void GPUPage::port_changed()
{
	Config::instance()->set_gpu_license_port(_port->GetValue());
}


void GPUPage::license_changed()
{
	Config::instance()->set_gpu_license(_license->get());
}


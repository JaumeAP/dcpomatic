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


#include "wx/about_dialog.h"
#include "wx/dcpomatic_button.h"
#include "wx/check_box.h"
#include "wx/editable_list.h"
#include "wx/i18n_setup.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/cross.h"
#include "lib/fix_audio_levels_job.h"
#include "lib/job_manager.h"
#include "lib/util.h"
#include "lib/variant.h"
#include <dcp/search.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/evtloop.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif


using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::vector;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** @file  src/tools/dcpomatic_processor.cc
 *  @brief A tool to batch-process DCPs in some way (e.g. fix their sound levels)
 */


class DirDialogWrapper : public wxDirDialog
{
public:
	DirDialogWrapper(wxWindow* parent)
		: wxDirDialog(parent, _("Choose a folder"), {}, wxDD_DIR_MUST_EXIST)
	{

	}

	vector<boost::filesystem::path> get() const
	{
		return dcp::find_potential_dcps(boost::filesystem::path(wx_to_std(GetPath())));
	}

	void set(boost::filesystem::path)
	{
		/* Not used */
	}
};



class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame(wxString const& title)
		: wxFrame(nullptr, -1, title)
		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		, _overall_panel(new wxPanel(this, wxID_ANY))
	{
		auto bar = new wxMenuBar;
		setup_menu(bar);
		SetMenuBar(bar);

		Bind(wxEVT_MENU, boost::bind(&DOMFrame::file_exit, this), wxID_EXIT);
		Bind(wxEVT_MENU, boost::bind(&DOMFrame::help_about, this), wxID_ABOUT);

#ifdef DCPOMATIC_WINDOWS
		SetIcon(wxIcon(std_to_wx("id")));
#endif
		auto overall_sizer = new wxBoxSizer(wxVERTICAL);

		auto input_dcp_sizer = new wxBoxSizer(wxHORIZONTAL);
		add_label_to_sizer(input_dcp_sizer, _overall_panel, _("Input DCPs"), true, 0, wxALIGN_CENTER_VERTICAL);

		auto input_dcps = new EditableList<boost::filesystem::path, DirDialogWrapper>(
			_overall_panel,
			{ EditableListColumn(_("DCP"), 300, true) },
			boost::bind(&DOMFrame::dcp_paths, this),
			boost::bind(&DOMFrame::set_dcp_paths, this, _1),
			[](boost::filesystem::path p, int) { return p.filename().string(); },
			EditableListTitle::INVISIBLE,
			EditableListButton::NEW | EditableListButton::REMOVE
			);

		input_dcp_sizer->Add(input_dcps, 1, wxLEFT | wxEXPAND, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(input_dcp_sizer, 0, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

		auto options_sizer = new wxBoxSizer(wxVERTICAL);
		_fix_audio_levels = new CheckBox(_overall_panel, _("Fix audio levels"));
		_fix_audio_levels->set(true);
		_fix_audio_levels->SetToolTip(
			_("Tick to change the audio levels to match the given LEQ(m) value")
		);
		options_sizer->Add(_fix_audio_levels, 0, wxBOTTOM, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(options_sizer, 0, wxLEFT, DCPOMATIC_DIALOG_BORDER);

		auto actions_sizer = new wxBoxSizer(wxHORIZONTAL);
		_cancel = new Button(_overall_panel, _("Cancel"));
		actions_sizer->Add(_cancel, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
		_process = new Button(_overall_panel, _("Process"));
		actions_sizer->Add(_process, 0, wxRIGHT, DCPOMATIC_SIZER_GAP);
		overall_sizer->Add(actions_sizer, 1, wxLEFT | wxRIGHT | wxALIGN_CENTER, DCPOMATIC_DIALOG_BORDER);

		_overall_panel->SetSizerAndFit(overall_sizer);

		_cancel->bind(&DOMFrame::cancel_clicked, this);
		_process->bind(&DOMFrame::process_clicked, this);

		setup_sensitivity();
	}

private:
	void file_exit()
	{
		Close();
	}

	void help_about()
	{
		AboutDialog dialog(_overall_panel);
		dialog.ShowModal();
	}

	void setup_menu(wxMenuBar* m)
	{
		auto help = new wxMenu;
#ifdef DCPOMATIC_OSX
		/* This just needs to be appended somewhere, it seems - it magically
		 * gets moved to the right place.
		 */
		help->Append(wxID_EXIT, _("&Exit"));
		help->Append(wxID_ABOUT, variant::wx::insert_dcpomatic(_("About %s")));
#else
		auto file = new wxMenu;
		file->Append(wxID_EXIT, _("&Quit"));
		m->Append(file, _("&File"));

		help->Append(wxID_ABOUT, _("About"));
#endif
		m->Append(help, _("&Help"));
	}

	void setup_sensitivity()
	{
		auto const work = JobManager::instance()->work_to_do();
		_cancel->Enable(work);
		_process->Enable(!_dcp_paths.empty() && !work);
	}

	void cancel_clicked()
	{
		_cancel_pending = true;
	}

	void process_clicked()
	{
		auto job_manager = JobManager::instance();
		vector<shared_ptr<const FixAudioLevelsJob>> jobs;
		for (auto const& dcp: _dcp_paths) {
			auto job = make_shared<FixAudioLevelsJob>(dcp, boost::filesystem::path("/home/carl/tmp/wankz"), 60, true);
			job_manager->add(job);
			jobs.push_back(job);
		}

		setup_sensitivity();

		while (job_manager->work_to_do() && !_cancel_pending) {
			wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
			dcpomatic_sleep_milliseconds(250);
		}

		if (_cancel_pending) {
			_cancel_pending = false;
			JobManager::instance()->cancel_all_jobs();
			setup_sensitivity();
			return;
		}

#if 0
		dcp::VerificationOptions options;
		options.check_picture_details = _check_picture_details->get();
		auto job_manager = JobManager::instance();
		vector<shared_ptr<const VerifyDCPJob>> jobs;
		for (auto const& dcp: _dcp_paths) {
			auto job = make_shared<VerifyDCPJob>(
				std::vector<boost::filesystem::path>{dcp},
				std::vector<boost::filesystem::path>(),
				options
			);
			job_manager->add(job);
			jobs.push_back(job);
		}

		setup_sensitivity();

		while (job_manager->work_to_do() && !_cancel_pending) {
			wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
			dcpomatic_sleep_milliseconds(250);
			auto last = job_manager->last_active_job();
			if (auto locked = last.lock()) {
				if (auto dcp = dynamic_pointer_cast<VerifyDCPJob>(locked)) {
					_progress_panel->update(dcp);
				}
			}
		}

		if (_cancel_pending) {
			_cancel_pending = false;
			JobManager::instance()->cancel_all_jobs();
			_progress_panel->clear();
			setup_sensitivity();
			return;
		}

		DCPOMATIC_ASSERT(_dcp_paths.size() == jobs.size());
		_result_panel->add(jobs);
		if (_write_log->get()) {
			for (size_t i = 0; i < _dcp_paths.size(); ++i) {
				dcp::TextFormatter formatter(_dcp_paths[i] / "REPORT.txt");
				dcp::verify_report({ jobs[i]->result() }, formatter);
			}
		}

		_progress_panel->clear();
		setup_sensitivity();
#endif
	}

private:
	void set_dcp_paths (vector<boost::filesystem::path> dcps)
	{
		_dcp_paths = dcps;
		setup_sensitivity();
	}

	vector<boost::filesystem::path> dcp_paths() const
	{
		return _dcp_paths;
	}

	wxPanel* _overall_panel = nullptr;
	std::vector<boost::filesystem::path> _dcp_paths;
	CheckBox* _fix_audio_levels;
	Button* _cancel;
	Button* _process;
	bool _cancel_pending = false;
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App()
		: wxApp()
	{
		dcpomatic_setup_path_encoding();
#ifdef DCPOMATIC_LINUX
		XInitThreads();
#endif
	}

private:
	bool OnInit() override
	{
		try {
			SetAppName(variant::wx::dcpomatic_processor());

			if (!wxApp::OnInit()) {
				return false;
			}

#ifdef DCPOMATIC_LINUX
			unsetenv("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
			dcpomatic_sleep_seconds(1);
			make_foreground_application();
#endif

			/* Enable i18n; this will create a Config object
			   to look for a force-configured language.  This Config
			   object will be wrong, however, because dcpomatic_setup
			   hasn't yet been called and there aren't any filters etc.
			   set up yet.
			*/
			dcpomatic::wx::setup_i18n();

			/* Set things up, including filters etc.
			   which will now be internationalised correctly.
			*/
			dcpomatic_setup();

			/* Force the configuration to be re-loaded correctly next
			   time it is needed.
			*/
			Config::drop();

			_frame = new DOMFrame(variant::wx::dcpomatic_processor());
			SetTopWindow(_frame);
			_frame->Maximize();
			_frame->Show();
		}
		catch (exception& e)
		{
			error_dialog(nullptr, variant::wx::insert_dcpomatic_processor(char_to_wx("%s could not start.")), std_to_wx(e.what()));
		}

		return true;
	}

	void report_exception()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (boost::filesystem::filesystem_error& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s) (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.path1().string()),
					std_to_wx(e.path2().string()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog(
				nullptr,
				wxString::Format(
					_("An exception occurred: %s.\n\n%s"),
					std_to_wx(e.what()),
					dcpomatic::wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), dcpomatic::wx::report_problem()));
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop() override
	{
		report_exception();
		return false;
	}

	void OnUnhandledException() override
	{
		report_exception();
	}

	DOMFrame* _frame = nullptr;
};


IMPLEMENT_APP(App)

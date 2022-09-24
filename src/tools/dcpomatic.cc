/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  src/tools/dcpomatic.cc
 *  @brief The main DCP-o-matic GUI.
 */


#include "wx/about_dialog.h"
#include "wx/content_panel.h"
#include "wx/dkdm_dialog.h"
#include "wx/export_subtitles_dialog.h"
#include "wx/export_video_file_dialog.h"
#include "wx/film_editor.h"
#include "wx/film_name_location_dialog.h"
#include "wx/film_viewer.h"
#include "wx/focus_manager.h"
#include "wx/full_config_dialog.h"
#include "wx/hints_dialog.h"
#include "wx/html_dialog.h"
#include "wx/i18n_hook.h"
#include "wx/job_manager_view.h"
#include "wx/kdm_dialog.h"
#include "wx/nag_dialog.h"
#include "wx/paste_dialog.h"
#include "wx/recreate_chain_dialog.h"
#include "wx/report_problem_dialog.h"
#include "wx/save_template_dialog.h"
#include "wx/self_dkdm_dialog.h"
#include "wx/send_i18n_dialog.h"
#include "wx/servers_list_dialog.h"
#include "wx/system_information_dialog.h"
#include "wx/templates_dialog.h"
#include "wx/update_dialog.h"
#include "wx/video_waveform_dialog.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "lib/analytics.h"
#include "lib/audio_content.h"
#include "lib/check_content_job.h"
#include "lib/cinema.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/dcpomatic_socket.h"
#include "lib/dkdm_wrapper.h"
#include "lib/emailer.h"
#include "lib/encode_server_finder.h"
#include "lib/exceptions.h"
#include "lib/ffmpeg_encoder.h"
#include "lib/film.h"
#include "lib/hints.h"
#include "lib/job_manager.h"
#include "lib/kdm_with_metadata.h"
#include "lib/log.h"
#include "lib/make_dcp.h"
#include "lib/release_notes.h"
#include "lib/screen.h"
#include "lib/send_kdm_email_job.h"
#include "lib/signal_manager.h"
#include "lib/subtitle_encoder.h"
#include "lib/text_content.h"
#include "lib/transcode_job.h"
#include "lib/update_checker.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/video_content.h"
#include <dcp/exceptions.h>
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/cmdline.h>
#include <wx/generic/aboutdlgg.h>
#include <wx/preferences.h>
#include <wx/splash.h>
#include <wx/stdpaths.h>
#include <wx/wxhtml.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif
#ifdef __WXMSW__
#include <shellapi.h>
#endif
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
/* This is OK as it's only used with DCPOMATIC_WINDOWS */
#include <sstream>

#ifdef check
#undef check
#endif


using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::function;
using std::list;
using std::make_pair;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;
using std::wcout;
using boost::optional;
using boost::is_any_of;
using boost::algorithm::find;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::raw_convert;


class FilmChangedClosingDialog
{
public:
	explicit FilmChangedClosingDialog (string name)
	{
		_dialog = new wxMessageDialog (
			nullptr,
			wxString::Format(_("Save changes to film \"%s\" before closing?"), std_to_wx (name).data()),
			/// TRANSLATORS: this is the heading for a dialog box, which tells the user that the current
			/// project (Film) has been changed since it was last saved.
			_("Film changed"),
			wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxICON_QUESTION
			);

		_dialog->SetYesNoCancelLabels (
			_("Save film and close"), _("Close without saving film"), _("Don't close")
			);
	}

	~FilmChangedClosingDialog ()
	{
		_dialog->Destroy ();
	}

	FilmChangedClosingDialog (FilmChangedClosingDialog const&) = delete;
	FilmChangedClosingDialog& operator= (FilmChangedClosingDialog const&) = delete;

	int run ()
	{
		return _dialog->ShowModal ();
	}

private:
	wxMessageDialog* _dialog;
};


class FilmChangedDuplicatingDialog
{
public:
	explicit FilmChangedDuplicatingDialog (string name)
	{
		_dialog = new wxMessageDialog (
			nullptr,
			wxString::Format(_("Save changes to film \"%s\" before duplicating?"), std_to_wx (name).data()),
			/// TRANSLATORS: this is the heading for a dialog box, which tells the user that the current
			/// project (Film) has been changed since it was last saved.
			_("Film changed"),
			wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxICON_QUESTION
			);

		_dialog->SetYesNoCancelLabels (
			_("Save film and duplicate"), _("Duplicate without saving film"), _("Don't duplicate")
			);
	}

	~FilmChangedDuplicatingDialog ()
	{
		_dialog->Destroy ();
	}

	FilmChangedDuplicatingDialog (FilmChangedDuplicatingDialog const&) = delete;
	FilmChangedDuplicatingDialog& operator= (FilmChangedDuplicatingDialog const&) = delete;

	int run ()
	{
		return _dialog->ShowModal ();
	}

private:
	wxMessageDialog* _dialog;
};


class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const& title)
		: wxFrame (nullptr, -1, title)
	{
		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		auto overall_panel = new wxPanel (this, wxID_ANY);

		_film_viewer.reset (new FilmViewer (overall_panel));
		_film_editor = new FilmEditor (overall_panel, _film_viewer);

		auto right_sizer = new wxBoxSizer (wxVERTICAL);
		right_sizer->Add (_film_viewer->panel(), 2, wxEXPAND | wxALL, 6);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (_film_editor, 0, wxEXPAND | wxALL, 6);
		main_sizer->Add (right_sizer, 1, wxEXPAND | wxALL, 6);

		overall_panel->SetSizer (main_sizer);
	}


	shared_ptr<Film> film () const {
		return _film;
	}

private:

	FilmEditor* _film_editor;
	std::shared_ptr<FilmViewer> _film_viewer;
	shared_ptr<Film> _film;
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
	{
#ifdef DCPOMATIC_LINUX
		XInitThreads ();
#endif
	}

private:

	bool OnInit () override
	{
		try {
			wxInitAllImageHandlers ();

			SetAppName (_("DCP-o-matic"));

			if (!wxApp::OnInit()) {
				return false;
			}

#ifdef DCPOMATIC_LINUX
			unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef DCPOMATIC_OSX
			dcpomatic_sleep_seconds (1);
			make_foreground_application ();
#endif

			dcpomatic_setup_path_encoding ();

			/* Enable i18n; this will create a Config object
			   to look for a force-configured language.  This Config
			   object will be wrong, however, because dcpomatic_setup
			   hasn't yet been called and there aren't any filters etc.
			   set up yet.
			*/
			dcpomatic_setup_i18n ();

			/* Set things up, including filters etc.
			   which will now be internationalised correctly.
			*/
			dcpomatic_setup ();

			/* Force the configuration to be re-loaded correctly next
			   time it is needed.
			*/
			Config::drop ();

			_frame = new DOMFrame (_("DCP-o-matic"));
			SetTopWindow (_frame);
			_frame->Maximize ();

			_frame->Show ();

			signal_manager = new wxSignalManager (this);
			Bind (wxEVT_IDLE, boost::bind (&App::idle, this, _1));

			_timer.reset (new wxTimer (this));
			_timer->Start (1000);
		}
		catch (exception& e)
		{
			if (_splash) {
				_splash->Destroy ();
				_splash = nullptr;
			}
			error_dialog (nullptr, wxString::Format ("DCP-o-matic could not start."), std_to_wx(e.what()));
		}

		return true;
	}

	void report_exception ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.file().string().c_str())
					)
				);
		} catch (boost::filesystem::filesystem_error& e) {
			error_dialog (
				nullptr,
				wxString::Format(
					_("An exception occurred: %s (%s) (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.path1().string()),
					std_to_wx (e.path2().string())
					)
				);
		} catch (exception& e) {
			error_dialog (
				nullptr,
				wxString::Format(
					_("An exception occurred: %s.\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what ())
					)
				);
		} catch (...) {
			error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop () override
	{
		report_exception ();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException () override
	{
		report_exception ();
	}

	void idle (wxIdleEvent& ev)
	{
		signal_manager->ui_idle ();
		ev.Skip ();
	}

	DOMFrame* _frame = nullptr;
	wxSplashScreen* _splash = nullptr;
	shared_ptr<wxTimer> _timer;
	string _film_to_load;
	string _film_to_create;
	string _content_to_add;
	string _dcp_to_add;
};


IMPLEMENT_APP (App)

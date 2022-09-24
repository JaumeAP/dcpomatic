#include "wx/film_editor.h"
#include "wx/film_viewer.h"
#include <wx/wx.h>

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

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (_film_viewer->panel(), 1, wxEXPAND);

		overall_panel->SetSizer (main_sizer);
	}

private:

	FilmEditor* _film_editor;
	std::shared_ptr<FilmViewer> _film_viewer;
};


/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
private:

	bool OnInit () override
	{
		if (!wxApp::OnInit()) {
			return false;
		}

		_frame = new DOMFrame (_("DCP-o-matic"));
		SetTopWindow (_frame);
		_frame->Maximize ();
		_frame->Show ();

		return true;
	}

	DOMFrame* _frame = nullptr;
};


IMPLEMENT_APP (App)

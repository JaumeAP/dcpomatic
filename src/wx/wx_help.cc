#include "wx_help.h"
#include "wx_util.h"
#include <iostream>

HelpGUI* HelpGUI::_instance = 0;


HelpGUI::HelpGUI ()
	: _current_index (-1)
	, _current_frame (0)
{
	_messages.push_back (Message("file_new", wxPoint(50, 100), Help::SUCCESSFUL_FILE_NEW));
	_messages.push_back (Message("add_content", ADD_FOLDER_BUTTON, wxSize(24, -48), Help::SUCCESSFUL_ADD_CONTENT));
	_messages.push_back (Message("preview", PLAY_BUTTON, wxSize(24, 0)));

	Help::instance()->Event.connect (boost::bind(&HelpGUI::event, this, _1));
}


void
HelpGUI::event (Help::EventType e)
{
	if (
		_current_index >= 0 &&
		_current_index < int(_messages.size() - 1) &&
		_messages[_current_index].next_trigger &&
		*_messages[_current_index].next_trigger == e) {
		next ();
	}
}


void
HelpGUI::landmark (wxWindow* landmark, Landmark id)
{
	_landmarks[id] = landmark;
}


void
HelpGUI::next ()
{
	++_current_index;
	show (_current_index);
}


void
HelpGUI::start ()
{
	next ();
}


void
HelpGUI::show (int index)
{
	if (_current_frame) {
		_current_frame->Destroy ();
		_current_frame = 0;
	}

	Message const& m = _messages[index];
	wxBitmap bitmap (bitmap_path("help/" + m.id));
	wxPoint pos;
	if (m.anchor_point) {
		pos = *m.anchor_point;
	} else if (m.anchor_landmark) {
		DCPOMATIC_ASSERT (_landmarks.find(*m.anchor_landmark) != _landmarks.end());
		wxWindow* lm = _landmarks[*m.anchor_landmark];
		pos = lm->GetScreenPosition();
		pos.x += lm->GetSize().GetWidth();
	}
	pos += m.offset;
	_current_frame = new wxFrame (0, wxID_ANY, wxT(""), pos, wxSize(bitmap.GetSize().GetWidth() + 16, bitmap.GetSize().GetHeight() + 128), wxFRAME_TOOL_WINDOW);
	wxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	wxStaticBitmap* static_bitmap = new wxStaticBitmap (_current_frame, wxID_ANY, bitmap);
	sizer->Add (static_bitmap, 1, wxEXPAND);
	wxSizer* buttons_sizer = new wxBoxSizer (wxHORIZONTAL);
	wxButton* close = new wxButton (_current_frame, wxID_ANY, _("Stop showing tips"));
	close->Bind (wxEVT_BUTTON, boost::bind(&HelpGUI::stop, this));
	buttons_sizer->Add (close, 1, wxEXPAND | wxALL, 8);
	sizer->Add (buttons_sizer, 0, wxALIGN_RIGHT);
	_current_frame->SetSizer (sizer);
	_current_frame->Show ();
}


void
HelpGUI::stop ()
{
	_current_index = -1;
	if (_current_frame) {
		_current_frame->Destroy ();
	}

	/* XXX: should write a config entry */
}


HelpGUI *
HelpGUI::instance ()
{
	if (!_instance) {
		_instance = new HelpGUI ();
	}

	return _instance;
}



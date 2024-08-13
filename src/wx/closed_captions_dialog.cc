/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

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


#include "closed_captions_dialog.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/compose.hpp"
#include "lib/constants.h"
#include "lib/film.h"
#include "lib/string_text.h"
#include "lib/text_content.h"
#include <boost/bind/bind.hpp>


using std::cout;
using std::list;
using std::make_pair;
using std::max;
using std::pair;
using std::shared_ptr;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


ClosedCaptionsDialog::ClosedCaptionsDialog (wxWindow* parent, FilmViewer* viewer)
	: wxDialog (parent, wxID_ANY, _("Closed captions"), wxDefaultPosition, wxDefaultSize,
#ifdef DCPOMATIC_OSX
		    /* I can't get wxFRAME_FLOAT_ON_PARENT to work on OS X, and although wxSTAY_ON_TOP keeps
		       the window above all others (and not just our own) it's better than nothing for now.
		    */
		    wxDEFAULT_FRAME_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxSTAY_ON_TOP
#else
		    wxDEFAULT_FRAME_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxFRAME_FLOAT_ON_PARENT
#endif
		)
	, _viewer (viewer)
	  /* XXX: empirical and probably unhelpful default size here; needs to be related to font metrics */
        , _display_panel(new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(640, (640 / 10) + 64)))
	, _track (new wxChoice(this, wxID_ANY))
	, _timer (this)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);

	wxBoxSizer* track_sizer = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (track_sizer, this, _("Track"), true, 0, wxLEFT | wxRIGHT | wxALIGN_CENTRE_VERTICAL);
	track_sizer->Add (_track, 0, wxEXPAND | wxLEFT, DCPOMATIC_SIZER_X_GAP);

	sizer->Add (track_sizer, 0, wxALL, DCPOMATIC_SIZER_GAP);
	sizer->Add(_display_panel, 1, wxEXPAND);

	Bind (wxEVT_SHOW, boost::bind(&ClosedCaptionsDialog::shown, this, _1));
	Bind (wxEVT_TIMER, boost::bind(&ClosedCaptionsDialog::update, this));
	_display_panel->Bind (wxEVT_PAINT, boost::bind(&ClosedCaptionsDialog::paint, this));
	_track->Bind (wxEVT_CHOICE, boost::bind(&ClosedCaptionsDialog::track_selected, this));

	SetSizerAndFit (sizer);
}

void
ClosedCaptionsDialog::shown (wxShowEvent ev)
{
	if (ev.IsShown ()) {
		_timer.Start (40);
	} else {
		_timer.Stop ();
	}
}

void
ClosedCaptionsDialog::track_selected ()
{
	_display.clear();
	_viewer->slow_refresh ();
	update ();
}

void
ClosedCaptionsDialog::paint ()
{
	wxPaintDC dc(_display_panel);
	dc.SetBackground (*wxBLACK_BRUSH);
	dc.Clear ();
	dc.SetTextForeground (*wxWHITE);

	/* Choose a font which fits vertically */
	int const line_height = max (8, dc.GetSize().GetHeight() / MAX_CLOSED_CAPTION_LINES);
	wxFont font (*wxNORMAL_FONT);
	font.SetPixelSize (wxSize (0, line_height * 0.8));
	dc.SetFont (font);

	auto current = _display.current();

	for (size_t i = 0; i < current.size(); ++i) {
		auto current_wx = std_to_wx(current[i]);
		auto const good = current_wx.Left(MAX_CLOSED_CAPTION_LENGTH);
		dc.DrawText (good, 8, line_height * i);
		if (current_wx.Length() > MAX_CLOSED_CAPTION_LENGTH) {
			auto const bad = current_wx.Right(current_wx.Length() - MAX_CLOSED_CAPTION_LENGTH);
			wxSize size = dc.GetTextExtent (good);
			dc.SetTextForeground (*wxRED);
			dc.DrawText (bad, 8 + size.GetWidth(), line_height * i);
			dc.SetTextForeground (*wxWHITE);
		}
	}
}

void
ClosedCaptionsDialog::update ()
{
	DCPOMATIC_ASSERT(_track->GetSelection() >= 0);
	DCPOMATIC_ASSERT(_track->GetSelection() < int(_tracks.size()));
	if (_display.update(_viewer->time(), _tracks.empty() ? boost::optional<DCPTextTrack>() : _tracks[_track->GetSelection()])) {
		Refresh();
	}
}

void
ClosedCaptionsDialog::clear ()
{
	_display.clear();
	Refresh ();
}


void
ClosedCaptionsDialog::set_butler (weak_ptr<Butler> butler)
{
	_display.set_butler(butler);
}

void
ClosedCaptionsDialog::update_tracks (shared_ptr<const Film> film)
{
	_tracks.clear ();

	for (auto i: film->content()) {
		for (auto j: i->text) {
			if (j->use() && j->type() == TextType::CLOSED_CAPTION && j->dcp_track()) {
				if (find(_tracks.begin(), _tracks.end(), j->dcp_track()) == _tracks.end()) {
					_tracks.push_back (*j->dcp_track());
				}
			}
		}
	}

	_track->Clear ();
	for (auto const& i: _tracks) {
		_track->Append (std_to_wx(String::compose("%1 (%2)", i.name, i.language ? i.language->to_string() : wx_to_std(_("Unknown")))));
	}

	if (_track->GetCount() > 0) {
		_track->SetSelection (0);
	}

	track_selected ();
}

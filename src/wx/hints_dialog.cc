/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "lib/film.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "lib/subtitle_content.h"
#include "lib/font.h"
#include "hints_dialog.h"
#include <wx/richtext/richtextctrl.h>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>

using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

HintsDialog::HintsDialog (wxWindow* parent, boost::weak_ptr<Film> film)
	: wxDialog (parent, wxID_ANY, _("Hints"))
	, _film (film)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	_text = new wxRichTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (400, 300), wxRE_READONLY);
	sizer->Add (_text, 1, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_text->GetCaret()->Hide ();

	boost::shared_ptr<Film> locked_film = _film.lock ();
	if (locked_film) {
		_film_changed_connection = locked_film->Changed.connect (boost::bind (&HintsDialog::film_changed, this));
		_film_content_changed_connection = locked_film->ContentChanged.connect (boost::bind (&HintsDialog::film_changed, this));
	}

	film_changed ();
}

void
HintsDialog::film_changed ()
{
	_text->Clear ();
	bool hint = false;

	boost::shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	ContentList content = film->content ();

	_text->BeginStandardBullet (N_("standard/circle"), 1, 50);

	bool big_font_files = false;
	if (film->interop ()) {
		BOOST_FOREACH (shared_ptr<Content> i, content) {
			shared_ptr<SubtitleContent> s = dynamic_pointer_cast<SubtitleContent> (i);
			if (s) {
				BOOST_FOREACH (shared_ptr<Font> j, s->fonts ()) {
					for (int i = 0; i < FontFiles::VARIANTS; ++i) {
						optional<boost::filesystem::path> const p = j->file (static_cast<FontFiles::Variant> (i));
						if (p && boost::filesystem::file_size (p.get()) >= (640 * 1024)) {
							big_font_files = true;
						}
					}
				}
			}
		}
	}

	if (big_font_files) {
		hint = true;
		_text->WriteText (_("You have specified a font file which is larger than 640kB.  This is very likely to cause problems on playback."));
	}

	int vob = 0;
	BOOST_FOREACH (shared_ptr<const Content> i, content) {
		if (boost::algorithm::starts_with (i->path(0).filename().string(), "VTS_")) {
			++vob;
		}
	}

	if (vob > 1) {
		hint = true;
		_text->WriteText (wxString::Format (_("You have %d files that look like they are VOB files from DVD. You should join them to ensure smooth joins between the files."), vob));
		_text->Newline ();
	}

	_text->EndSymbolBullet ();

	if (!hint) {
		_text->WriteText (_("There are no hints: everything looks good!"));
	}
}

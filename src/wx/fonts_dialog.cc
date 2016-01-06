/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "fonts_dialog.h"
#include "wx_util.h"
#include "system_font_dialog.h"
#include "font_files_dialog.h"
#include "lib/font.h"
#include "lib/subtitle_content.h"
#include "lib/cross.h"
#include <wx/wx.h>
#include <wx/imaglist.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::string;
using std::cout;
using boost::shared_ptr;
using boost::optional;

FontsDialog::FontsDialog (wxWindow* parent, shared_ptr<SubtitleContent> content)
	: wxDialog (parent, wxID_ANY, _("Fonts"))
	, _content (content)
{
	_fonts = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxSize (550, 200), wxLC_REPORT | wxLC_SINGLE_SEL);

	wxImageList* image_list = new wxImageList ();
	boost::filesystem::path p = shared_path() / "warning.png";
	wxBitmap warning;
	warning.LoadFile (std_to_wx(p.string()), wxBITMAP_TYPE_PNG);
	image_list->Add (warning);
	_fonts->AssignImageList (image_list, wxIMAGE_LIST_SMALL);

	{
		wxListItem ip;
		ip.SetId (0);
		ip.SetText (_("ID"));
		ip.SetWidth (100);
		_fonts->InsertColumn (0, ip);
	}

	{
		wxListItem ip;
		ip.SetId (1);
		ip.SetText (_("Normal file"));
		ip.SetWidth (150);
		_fonts->InsertColumn (1, ip);
	}

	{
		wxListItem ip;
		ip.SetId (2);
		ip.SetText (_("Italic file"));
		ip.SetWidth (150);
		_fonts->InsertColumn (2, ip);
	}

	{
		wxListItem ip;
		ip.SetId (3);
		ip.SetText (_("Bold file"));
		ip.SetWidth (150);
		_fonts->InsertColumn (3, ip);
	}

	wxBoxSizer* sizer = new wxBoxSizer (wxHORIZONTAL);
	sizer->Add (_fonts, 1, wxEXPAND | wxLEFT | wxRIGHT, DCPOMATIC_SIZER_X_GAP);

	_edit = new wxButton (this, wxID_ANY, _("Edit..."));
	sizer->Add (_edit, 0, wxTOP | wxBOTTOM, DCPOMATIC_BUTTON_STACK_GAP);

	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	overall_sizer->Add (sizer, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizerAndFit (overall_sizer);

	_edit->Bind (wxEVT_COMMAND_BUTTON_CLICKED, boost::bind (&FontsDialog::edit_clicked, this));
	_fonts->Bind (wxEVT_COMMAND_LIST_ITEM_SELECTED, boost::bind (&FontsDialog::selection_changed, this));
	_fonts->Bind (wxEVT_COMMAND_LIST_ITEM_DESELECTED, boost::bind (&FontsDialog::selection_changed, this));

	setup ();
}

void
FontsDialog::set_variant (int n, shared_ptr<Font> font, FontFiles::Variant variant)
{
	if (font->file(variant)) {
		wxListItem item;

		item.SetId (n);
		optional<boost::filesystem::path> const p = font->file (variant);
		if (p && boost::filesystem::file_size (p.get()) >= (640 * 1024)) {
			item.SetImage (0);
		}
		item.SetText (font->file(variant).get().leaf().string ());

		switch (variant) {
		case FontFiles::NORMAL:
			_fonts->SetColumn (1, item);
			break;
		case FontFiles::ITALIC:
			_fonts->SetColumn (2, item);
			break;
		case FontFiles::BOLD:
			_fonts->SetColumn (3, item);
			break;
		default:
			DCPOMATIC_ASSERT (false);
		}
	}
}

void
FontsDialog::setup ()
{
	shared_ptr<SubtitleContent> content = _content.lock ();
	if (!content) {
		return;
	}

	_fonts->DeleteAllItems ();
	size_t n = 0;
	BOOST_FOREACH (shared_ptr<Font> i, content->fonts ()) {
		wxListItem item;
		item.SetId (n);
		_fonts->InsertItem (item);
		_fonts->SetItem (n, 0, std_to_wx (i->id ()));
		set_variant (n, i, FontFiles::NORMAL);
		set_variant (n, i, FontFiles::BOLD);
		set_variant (n, i, FontFiles::ITALIC);
		++n;
	}

	setup_sensitivity ();
}

void
FontsDialog::selection_changed ()
{
	setup_sensitivity ();
}

void
FontsDialog::setup_sensitivity ()
{
	int const item = _fonts->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	_edit->Enable (item != -1);
}

void
FontsDialog::edit_clicked ()
{
	shared_ptr<SubtitleContent> content = _content.lock ();
	if (!content) {
		return;
	}

	int const item = _fonts->GetNextItem (-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	string const id = wx_to_std (_fonts->GetItemText (item, 0));
	shared_ptr<Font> font;
	BOOST_FOREACH (shared_ptr<Font> i, content->fonts()) {
		if (i->id() == id) {
			font = i;
		}
	}

	if (!font) {
		return;
	}

	FontFilesDialog* d = new FontFilesDialog (this, font->files ());
	if (d->ShowModal () == wxID_OK) {
		font->set_files (d->get ());
	}
	d->Destroy ();

	setup ();
}

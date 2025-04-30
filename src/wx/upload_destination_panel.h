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


#include "lib/upload_destination.h"
#include <wx/treelist.h>
#include <wx/wx.h>
#include <boost/signals2.hpp>
#include <map>


class Button;


class UploadDestinationPanel : public wxPanel
{
public:
	UploadDestinationPanel(wxWindow* parent);

	std::vector<UploadDestination> destinations() const;

	boost::signals2::signal<void ()> DestinationsChanged;

private:
	void add();
	void edit();
	void remove();

	void add_destination(UploadDestination destination);
	boost::optional<wxTreeListItem> selected() const;
	void selection_changed();
	void setup_sensitivity();
	void update_config();
	void destinations_changed();
	void checkbox_changed();

	wxTreeListCtrl* _list;
	Button* _add;
	Button* _edit;
	Button* _remove;
	std::vector<std::pair<wxTreeListItem, UploadDestination>> _destinations;
};


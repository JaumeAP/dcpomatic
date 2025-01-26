/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "cucumber_registry.h"
#include "lib/dcpomatic_assert.h"
#include <wx/wx.h>


using std::map;
using std::string;


CucumberRegistry* CucumberRegistry::_instance = 0;


CucumberRegistry *
CucumberRegistry::instance()
{
	if (!_instance) {
		_instance = new CucumberRegistry();
	}

	return _instance;
}


void
CucumberRegistry::add(string id, wxButton* button)
{
	_buttons[id] = button;
}


void
CucumberRegistry::remove(wxButton* button)
{
	for (map<string, wxButton*>::iterator i = _buttons.begin(); i != _buttons.end(); ++i) {
		if (i->second == button) {
			_buttons.erase(i);
			return;
		}
	}
}


void
CucumberRegistry::click_button(string id)
{
	auto i = _buttons.find(id);
	DCPOMATIC_ASSERT(i != _buttons.end());
	wxPostEvent(i->second, wxCommandEvent(wxEVT_BUTTON));
}


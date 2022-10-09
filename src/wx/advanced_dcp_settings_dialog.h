/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


LIBDCP_DISABLE_WARNINGS
#include <wx/dialog.h>
LIBDCP_ENABLE_WARNINGS
#include <memory>


class CheckBox;
class Film;


class AdvancedDCPSettingsDialog : public wxDialog
{
public:
	AdvancedDCPSettingsDialog(wxWindow* parent, std::shared_ptr<const Film> film);

	bool write_cpl_metadata() const;

private:
	CheckBox* _write_cpl_metadata;
};


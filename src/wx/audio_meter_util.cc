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


#include "audio_meter_util.h"
#include <wx/wx.h>


using std::pair;
using std::vector;


auto constexpr min_db = -70.0f;

static auto const _scales = std::vector<std::pair<int, wxString>>{
	{ 0, wxT("0dB") },
	{ -10, wxT("-10dB") },
	{ -20, wxT("-20dB") },
	{ -30, wxT("-30dB") },
	{ -40, wxT("-40dB") },
	{ -50, wxT("-50dB") },
	{ -60, wxT("-60dB") },
	{ -70, wxT("-70dB") },
};



int
dcpomatic::audio_meter::db_to_y(int meter_height, float db)
{
	return meter_height - (db - min_db) * meter_height / -min_db;
}


float
dcpomatic::audio_meter::linear_level_to_db(float level)
{
	return std::max(min_db, 20 * log10f(level));

}


vector<pair<int, wxString>> const&
dcpomatic::audio_meter::scales()
{
	return _scales;
}


int
dcpomatic::audio_meter::border()
{
	return 16;
}


int
dcpomatic::audio_meter::scale_label_width()
{
	return 28;
}


int
dcpomatic::audio_meter::channel_label_height()
{
	return 24;
}


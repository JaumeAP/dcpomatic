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


#include "simple_meters.h"
#include "wx_util.h"
#include "lib/level_calculator.h"
#include <dcp/types.h>
#include <wx/wx.h>
#include <algorithm>


static auto constexpr min_db = -70.0f;


using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


SimpleMeters::SimpleMeters(wxWindow* parent)
	: _panel(new wxPanel(parent, wxID_ANY))
{
	_panel->Bind(wxEVT_PAINT, boost::bind(&SimpleMeters::paint, this));
}


wxWindow*
SimpleMeters::get() const
{
	return _panel;
}


void
SimpleMeters::paint()
{
	wxPaintDC dc(_panel);

	dc.SetBackground(*wxBLACK_BRUSH);
	dc.Clear();

	if (_peaks.empty()) {
		return;
	}

	auto const scale = 1 / dpi_scale_factor(_panel);
	dc.SetLogicalScale(scale, scale);

	auto const panel_size = dcp::Size(_panel->GetSize().GetWidth() / scale, _panel->GetSize().GetHeight() / scale);

	auto const channel_width = panel_size.width / _peaks.size();

	int x = 0;
	dc.SetBrush(*wxGREEN_BRUSH);
	for (auto peak: _peaks) {
		auto const peak_db = std::max(min_db, 20 * log10(peak));
		dc.DrawRectangle(x, panel_size.height, channel_width, (1 - (peak_db - min_db)) * panel_size.height / (-min_db));
		x += channel_width;
	}
}


bool
SimpleMeters::update(shared_ptr<LevelCalculator> calculator, dcpomatic::DCPTime time)
{
	if (!_panel->GetParent()->IsShown()) {
		return false;
	}

	_peaks = calculator->get(time);
	_panel->Refresh();
	_panel->Update();

	return true;
}


void
SimpleMeters::clear()
{
	_peaks.clear();
	_panel->Refresh();
	_panel->Update();
}

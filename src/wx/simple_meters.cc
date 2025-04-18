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
#include "lib/constants.h"
#include "lib/level_calculator.h"
#include "lib/util.h"
#include <dcp/types.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
LIBDCP_ENABLE_WARNINGS
#include <wx/wx.h>
#include <algorithm>


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
	auto constexpr min_db = -70.0f;
	auto constexpr border = 16;
	auto constexpr scale_label_width = 28;

	wxPaintDC dc(_panel);

	dc.SetBackground(*wxBLACK_BRUSH);
	dc.Clear();

	auto const scale = 1 / dpi_scale_factor(_panel);
	auto const panel_size = dcp::Size(_panel->GetSize().GetWidth() / scale, _panel->GetSize().GetHeight() / scale);
	auto const meter_height = panel_size.height - 24 - 2 * border;
	dc.SetLogicalScale(scale, scale);

	auto db_to_y = [meter_height](float db) {
		return border + meter_height - (db - min_db) * meter_height / -min_db;
	};

	static auto const scales = std::vector<std::pair<int, wxString>>{
		{ 0, wxT("0dB") },
		{ -10, wxT("-10dB") },
		{ -20, wxT("-20dB") },
		{ -30, wxT("-30dB") },
		{ -40, wxT("-40dB") },
		{ -50, wxT("-50dB") },
		{ -60, wxT("-60dB") },
		{ -70, wxT("-70dB") },
	};

	auto gc = wxGraphicsContext::Create(dc);
	if (!gc) {
		return;
	}

	gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
	gc->SetFont(gc->CreateFont(*wxSMALL_FONT, *wxWHITE));

	/* Scale lines and labels (0dB, -10dB etc.) */
	dc.SetPen(*wxGREY_PEN);
	for (auto const& scale: scales) {
		auto const y = static_cast<int>(std::round(db_to_y(scale.first)));
		dc.DrawLine({ border + scale_label_width, y }, { panel_size.width - border, y });
		auto const size = _sizes.get(gc, scale.second);
		gc->DrawText(scale.second, border + scale_label_width - size.width - 4, y - size.height / 2);
	}

	if (_peaks.empty()) {
		delete gc;
		return;
	}

	auto const channel_width = (panel_size.width - 2 * border - scale_label_width) / _peaks.size();

	/* Meter bars */
	int x = border + scale_label_width;
	dc.SetBrush(*wxGREEN_BRUSH);
	dc.SetPen(*wxBLACK_PEN);
	for (auto peak: _peaks) {
		auto const peak_db = std::max(min_db, 20 * log10(peak));
		auto const y = db_to_y(peak_db);
		dc.DrawRectangle(x, y, channel_width, meter_height - y + border);
		x += channel_width;
	}

	/* Channel name labels */
	for (auto i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		auto name = std_to_wx(short_audio_channel_name(i));
		gc->DrawText(name, border + scale_label_width + i * channel_width + (channel_width - _sizes.get(gc, name).width) / 2, panel_size.height - 24);
	}

	delete gc;
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

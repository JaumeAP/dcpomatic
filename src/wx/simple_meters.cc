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
	auto const meter_height = panel_size.height - 24;

	int x = 0;
	dc.SetBrush(*wxGREEN_BRUSH);
	for (auto peak: _peaks) {
		auto const peak_db = std::max(min_db, 20 * log10(peak));
		auto const height = (peak_db - min_db) * meter_height / -min_db;
		dc.DrawRectangle(x, meter_height - height, channel_width, height);
		x += channel_width;
	}

	auto gc = wxGraphicsContext::Create(dc);
	if (!gc) {
		return;
	}

	gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
	gc->SetFont(gc->CreateFont(*wxSMALL_FONT, *wxWHITE));

	if (_label_widths.empty()) {
		_label_widths.resize(MAX_DCP_AUDIO_CHANNELS);
		for (auto i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
			wxDouble label_width;
			wxDouble label_height;
			wxDouble label_descent;
			wxDouble label_leading;
			gc->GetTextExtent(std_to_wx(short_audio_channel_name(i)), &label_width, &label_height, &label_descent, &label_leading);
			_label_widths[i] = label_width;
		}
	}

	for (auto i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		gc->DrawText(std_to_wx(short_audio_channel_name(i)), i * channel_width + (channel_width - _label_widths[i]) / 2, panel_size.height - 24);
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

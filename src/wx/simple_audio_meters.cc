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
#include "simple_audio_meters.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/constants.h"
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


SimpleAudioMeters::SimpleAudioMeters(wxWindow* parent)
	: _panel(new wxPanel(parent, wxID_ANY))
{
	_panel->Bind(wxEVT_PAINT, boost::bind(&SimpleAudioMeters::paint, this));
}


wxWindow*
SimpleAudioMeters::get() const
{
	return _panel;
}


void
SimpleAudioMeters::paint()
{
	using namespace dcpomatic::audio_meter;

	wxPaintDC dc(_panel);

	dc.SetBackground(*wxBLACK_BRUSH);
	dc.Clear();

	auto const scale = 1 / dpi_scale_factor(_panel);
	auto const panel_size = dcp::Size(_panel->GetSize().GetWidth() / scale, _panel->GetSize().GetHeight() / scale);
	auto const meter_height = panel_size.height - channel_label_height() - 2 * border();
	dc.SetLogicalScale(scale, scale);

	auto gc = wxGraphicsContext::Create(dc);
	if (!gc) {
		return;
	}

	gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
	gc->SetFont(gc->CreateFont(*wxSMALL_FONT, *wxWHITE));

	/* Scale lines and labels (0dB, -10dB etc.) */
	dc.SetPen(*wxGREY_PEN);
	for (auto const& scale: dcpomatic::audio_meter::scales()) {
		auto const y = static_cast<int>(std::round(border() + db_to_y(meter_height, scale.first)));
		dc.DrawLine({ border() + scale_label_width(), y }, { panel_size.width - border(), y });
		auto const size = _sizes.get(gc, scale.second);
		gc->DrawText(scale.second, border() + scale_label_width() - size.width - 4, y - size.height / 2);
	}

	if (!_levels) {
		delete gc;
		return;
	}

	auto const channel_width = (panel_size.width - 2 * border() - scale_label_width()) / _levels->peaks.size();

	/* Meter bars */
	int x = border() + scale_label_width();
	dc.SetBrush(*wxGREEN_BRUSH);
	dc.SetPen(*wxBLACK_PEN);
	for (auto peak: _levels->peaks) {
		auto const y = border() + db_to_y(meter_height, linear_level_to_db(peak));
		dc.DrawRectangle(x, y, channel_width, meter_height - y + border());
		x += channel_width;
	}

	/* Channel name labels */
	for (auto i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		auto name = std_to_wx(short_audio_channel_name(i));
		gc->DrawText(name, border() + scale_label_width() + i * channel_width + (channel_width - _sizes.get(gc, name).width) / 2, panel_size.height - channel_label_height());
	}

	delete gc;
}


void
SimpleAudioMeters::update(shared_ptr<Butler> butler, dcpomatic::DCPTime time)
{
	while (!_levels || (_levels->time + butler->time_between_audio_level_measurements()) < time) {
		_levels = butler->get_audio_levels();
		if (!_levels) {
			return;
		}
	}

	_panel->Refresh();
	_panel->Update();
}


void
SimpleAudioMeters::clear()
{
	_levels = {};
	_panel->Refresh();
	_panel->Update();
}

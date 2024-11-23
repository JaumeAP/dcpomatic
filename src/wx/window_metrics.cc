/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "window_metrics.h"
#include "lib/config.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/app.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/bind/bind.hpp>


#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


WindowMetrics::WindowMetrics(std::string name, wxSize default_size, wxWindow* window)
	: _name(name)
	, _window(window)
{
	if (auto metrics = Config::instance()->window_metrics(name)) {
		_position = { metrics->first.x, metrics->first.y };
		_size = { metrics->second.width, metrics->second.height };
	} else {
		_position = wxDefaultPosition;
		_size = default_size;
	}
}


void
WindowMetrics::bind()
{
	_window->Bind(wxEVT_SIZE, boost::bind(&WindowMetrics::sized, this, _1));
	_window->Bind(wxEVT_MOVE, boost::bind(&WindowMetrics::moved, this, _1));
}


void
WindowMetrics::show() const
{
#ifdef DCPOMATIC_LINUX
	auto const position_before = _position;

	_window->SetSize(_size);
	_window->Show();

	wxTheApp->CallAfter([this, position_before] {
		_window->SetPosition(position_before);
	});
#else
	_window->SetSize(_size);
	_window->SetPosition(_position);
	_window->Show();
#endif
}


void
WindowMetrics::write_to_config() const
{
	Config::instance()->set_window_metrics(_name, { _position.x, _position.y }, { _size.GetWidth(), _size.GetHeight() });
}


void
WindowMetrics::sized(wxSizeEvent& ev)
{
	auto top_level_window = dynamic_cast<wxTopLevelWindow*>(_window);
	if (top_level_window && top_level_window->IsMaximized()) {
		/* On Windows the maximised state looks a little different to the
		 * window just being as big as it can be; set wxDefaultSize here
		 * so that we call Maximize in show().
		 */
		_size = wxDefaultSize;
	} else {
		_size = ev.GetSize();
	}
	write_to_config();
	ev.Skip();
}


void
WindowMetrics::moved(wxMoveEvent& ev)
{
#ifdef DCPOMATIC_LINUX
	_position = _window->ClientToScreen({0, 0});
#else
	int x;
	int y;
	_window->GetScreenPosition(&x, &y);
	_position = { x, y };
#endif
	write_to_config();
	ev.Skip();
}


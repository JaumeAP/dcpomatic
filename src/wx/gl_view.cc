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


#include "lib/dcpomatic_log.h"
#include "gl_view.h"
#include <wx/wx.h>

#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/* This will only build on an new-enough wxWidgets: see the comment in gl_view.h */
#if wxCHECK_VERSION(3,1,0)


GLView::GLView(wxWindow* parent)
	: _context(nullptr)
{
	wxGLAttributes attributes;
	/* We don't need a depth buffer, and indeed there is apparently a bug with Windows/Intel HD 630
	 * which puts green lines over the OpenGL display if you have a non-zero depth buffer size.
	 * https://community.intel.com/t5/Graphics/Request-for-details-on-Intel-HD-630-green-lines-in-OpenGL-apps/m-p/1202179
	 */
	attributes.PlatformDefaults().MinRGBA(8, 8, 8, 8).DoubleBuffer().Depth(0).EndList();
	_canvas = new wxGLCanvas(
		parent, attributes, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE
	);
	_canvas->Bind(wxEVT_SIZE, boost::bind(&GLView::size_changed, this, _1));
}


void
GLView::size_changed(wxSizeEvent const& ev)
{
	auto const scale = _canvas->GetDPIScaleFactor();
	int const width = std::round(ev.GetSize().GetWidth() * scale);
	int const height = std::round(ev.GetSize().GetHeight() * scale);
	_canvas_size = { width, height };
	LOG_GENERAL("GLView canvas size changed to %1x%2", width, height);
}


#endif


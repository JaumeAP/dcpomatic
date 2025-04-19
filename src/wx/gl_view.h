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


#include "lib/exception_store.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/glcanvas.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/atomic.hpp>
#include <boost/thread.hpp>


/* The OpenGL API in wxWidgets 3.0.x is sufficiently different to make it awkward to support,
 * and I think it may even have things missing that we require (e.g. the attributes parameter
 * to wxGLContext).  Hence we only support the GLView on wxWidgets 3.1.0 and higher
 * (which only excludes the old macOS builds, since wxWidgets 3.1.x does not support macOS
 * 10.9 or earlier).
 */
#if wxCHECK_VERSION(3,1,0)


class GLView : public ExceptionStore
{
public:
	GLView(wxWindow* parent);
	virtual ~GLView();

	wxWindow* canvas() const {
		return _canvas;
	}

	bool vsync_enabled() const {
		return _vsync_enabled;
	}

protected:
	virtual void size_changed(wxSizeEvent const& ev);
	virtual void thread_setup() = 0;
	virtual void thread_body() = 0;

	void start_thread_if_required();

	wxGLCanvas* _canvas;
	boost::atomic<wxSize> _canvas_size;
	wxGLContext* _context;

private:
	void thread();

	boost::thread _thread;
	bool _vsync_enabled;
};


#endif

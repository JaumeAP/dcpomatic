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


#include "lib/cross.h"
#include "lib/dcpomatic_log.h"
#include "lib/exceptions.h"
#include "gl_view.h"
#include <wx/wx.h>

#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/* This will only build on an new-enough wxWidgets: see the comment in gl_view.h */
#if wxCHECK_VERSION(3,1,0)


GLView::GLView(wxWindow* parent)
	: _context(nullptr)
	, _vsync_enabled(false)
	, _playing(false)
	, _one_shot(false)
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


GLView::~GLView()
{
	boost::this_thread::disable_interruption dis;

	try {
		_thread.interrupt();
		_thread.join();
	} catch (...) {}
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


void
GLView::start_thread_if_required()
{
	if (!_thread.joinable()) {
		_thread = boost::thread(boost::bind(&GLView::thread, this));
	}
}


void
GLView::thread()
try
{
	thread_setup();

#if defined(DCPOMATIC_LINUX) && defined(DCPOMATIC_HAVE_GLX_SWAP_INTERVAL_EXT)
	if (_canvas->IsExtensionSupported("GLX_EXT_swap_control")) {
		/* Enable vsync */
		Display* dpy = wxGetX11Display();
		glXSwapIntervalEXT(dpy, DefaultScreen(dpy), 1);
		_vsync_enabled = true;
	}
#endif

#ifdef DCPOMATIC_WINDOWS
	if (_canvas->IsExtensionSupported("WGL_EXT_swap_control")) {
		/* Enable vsync */
		PFNWGLSWAPINTERVALEXTPROC swap = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
		if (swap) {
			swap(1);
			_vsync_enabled = true;
		}
	}

#endif

#ifdef DCPOMATIC_OSX
	/* Enable vsync */
	GLint swapInterval = 1;
	CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &swapInterval);
	_vsync_enabled = true;
#endif

	while (true) {
		boost::mutex::scoped_lock lm(_playing_mutex);
		while (!_playing && !_one_shot) {
			_thread_work_condition.wait(lm);
		}
		lm.unlock();

		if (_playing) {
			thread_playing();
		} else if (_one_shot) {
			_one_shot = false;
			draw();
		}

		boost::this_thread::interruption_point();
		dcpomatic_sleep_milliseconds(time_until_next_frame().get_value_or(0));
	}
}
catch (boost::thread_interrupted&)
{

}
catch (...)
{
	store_current();
}


void
GLView::start()
{
	boost::mutex::scoped_lock lm(_playing_mutex);
	_playing = true;
	_thread_work_condition.notify_all();
}


void
GLView::stop()
{
	boost::mutex::scoped_lock lm(_playing_mutex);
	_playing = false;
}


void
GLView::request_one_shot()
{
	boost::mutex::scoped_lock lm(_playing_mutex);
	_one_shot = true;
	_thread_work_condition.notify_all();
}


void
GLView::update()
{
	if (!_canvas->IsShownOnScreen()) {
		return;
	}

	/* It appears important to do this from the GUI thread; if we do it from the GL thread
	 * on Linux we get strange failures to create the context for any version of GL higher
	 * than 3.2.
	 */
	ensure_context();

#ifdef DCPOMATIC_OSX
	/* macOS gives errors if we don't do this (and therefore [NSOpenGLContext setView:]) from the main thread */
	if (!_setup_shaders_done) {
		setup_shaders();
		_setup_shaders_done = true;
	}
#endif

	start_thread_if_required();
	request_one_shot();
	rethrow();
}


void
GLView::ensure_context()
{
	if (!_context) {
		wxGLContextAttrs attrs;
		attrs.PlatformDefaults().CoreProfile().OGLVersion(4, 1).EndList();
		_context = new wxGLContext(_canvas, nullptr, &attrs);
		if (!_context->IsOK()) {
			boost::throw_exception(GLError("Making GL context", -1));
		}
	}
}



#endif


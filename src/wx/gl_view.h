/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_GL_VIEW_H
#define DCPOMATIC_GL_VIEW_H


#include "lib/exception_store.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/glcanvas.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <atomic>


/* The OpenGL API in wxWidgets 3.0.x is sufficiently different to make it awkward to support,
 * and I think it may even have things missing that we require (e.g. the attributes parameter
 * to wxGLContext).  Hence we only support the GLVideoView on wxWidgets 3.1.0 and higher
 * (which only excludes the old macOS builds, since wxWidgets 3.1.x does not support macOS
 * 10.9 or earlier).
 */
#if wxCHECK_VERSION(3,1,0)

class wxGLCanvas;
class wxWindow;


class GLView : public ExceptionStore
{
public:
	GLView(
		wxWindow* parent,
		char const* fragment_source,
		std::function<void ()> setup_shaders,
		std::function<void ()> setup_textures,
		std::function<void ()> thread_body,
		std::function<void ()> update
	);

	~GLView();

	wxGLCanvas* get() const {
		return _canvas;
	}

	wxSize size() const {
		return _canvas_size.load();
	}

	bool vsync_enabled() const {
		return _vsync_enabled;
	}

	unsigned int program() const {
		return _program;
	}

	GLuint vao() const {
		return _vao;
	}

	boost::signals2::signal<void()> Sized;

private:
	void size_changed(wxSizeEvent const &);
	void show_changed(wxShowEvent const &);
	void update();
	void thread();
	void ensure_shaders();

	wxGLCanvas* _canvas;
	std::atomic<wxSize> _canvas_size;
	wxGLContext* _context = nullptr;

	boost::thread _thread;

	bool _setup_shaders_done = false;
	bool _vsync_enabled = false;
	GLuint _vao = 0;
	GLuint _program = 0;
	std::string _fragment_source;

	std::function<void ()> _setup_shaders;
	std::function<void ()> _setup_textures;
	std::function<void ()> _thread_body;
	std::function<void ()> _update;
};

#endif

#endif

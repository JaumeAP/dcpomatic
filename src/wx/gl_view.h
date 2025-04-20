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


#ifndef DCPOMATIC_GL_VIEW_H
#define DCPOMATIC_GL_VIEW_H


#include "lib/exception_store.h"
#include <wx/wx.h>
#include <boost/atomic.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>


/* The OpenGL API in wxWidgets 3.0.x is sufficiently different to make it awkward to support,
 * and I think it may even have things missing that we require (e.g. the attributes parameter
 * to wxGLContext).  Hence we only support the GLVideoView on wxWidgets 3.1.0 and higher
 * (which only excludes the old macOS builds, since wxWidgets 3.1.x does not support macOS
 * 10.9 or earlier).
 */
#if wxCHECK_VERSION(3,1,0)


class wxGLCanvas;
class wxGLContext;


/** @class GLView
 *
 *  @brief An OpenGL view for things that can be "played".
 */
class GLView : public ExceptionStore
{
public:
	/** @param parent Parent window for the wxGLCanvas.
	 *  @param setup_shaders Function to set up any GL shaders that are required.
	 *  May be called from any thread.
	 *  @param create_textures Function to create any GL textures that are required.
	 *  Will be called from the GLView's internal thread.
	 *  @param play_pre_draw Function called when the view is playing, before draw is called
	 *  to draw a frame.  Should prepare things so that draw renders a new frame.
	 *  @param draw function to render the current frame.
	 *  @param play_pre_draw Function called after draw when playing.  Should return the number
	 *  of ms to sleep before trying to draw the next frame.
	 */
	GLView(
		wxWindow* parent,
		std::function<void ()> setup_shaders,
		std::function<void ()> create_textures,
		std::function<void ()> play_pre_draw,
		std::function<void ()> draw,
		std::function<int ()> play_post_draw,
		std::string fragment_source
	);

	~GLView();

	wxGLCanvas* canvas() const {
		return _canvas;
	}

	/** Can be called from any thread */
	wxSize size() const;

	/** Can be called from any thread */
	bool vsync_enabled() const {
		return _vsync_enabled;
	}

	/** Arrange for `draw' to be called soon to update the view.
	 *  Must be called from the UI thread.
	 */
	void request_draw();

	void start();
	void stop();

	unsigned int program() const {
		return _program;
	}

	/** Emitted from the UI thread when the view changes in size */
	boost::signals2::signal<void()> Sized;

private:
	void size_changed(wxSizeEvent const& ev);
	void ensure_context();
	void thread();
	void setup_shaders();
	void draw();

	wxGLCanvas* _canvas;
	wxGLContext* _context = nullptr;

	boost::atomic<wxSize> _canvas_size;
	bool _setup_shaders_done = false;
	boost::atomic<bool> _vsync_enabled;

	unsigned int _vao;
	unsigned int _program;

	boost::thread _thread;
	boost::mutex _playing_mutex;
	boost::condition _thread_work_condition;
	boost::atomic<bool> _playing;
	boost::atomic<bool> _draw_requested;

	std::function<void ()> _setup_shaders;
	std::function<void ()> _create_textures;
	std::function<void ()> _play_pre_draw;
	std::function<void ()> _draw;
	std::function<int ()> _play_post_draw;
	std::string _fragment_source;
};


#endif

#endif

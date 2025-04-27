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


#include "gl_util.h"
#include "gl_view.h"
#include "lib/dcpomatic_log.h"
#include "lib/exceptions.h"
#include "lib/util.h"
LIBDCP_DISABLE_WARNINGS
#include <wx/glcanvas.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS


using std::string;
using namespace dcpomatic::gl;


/* This will only build on an new-enough wxWidgets: see the comment in gl_view.h */
#if wxCHECK_VERSION(3,1,0)


using std::function;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


GLView::GLView(
	wxWindow* parent,
	function<void ()> setup_shaders,
	function<void ()> create_textures,
	function<void ()> play_pre_draw,
	function<void ()> draw,
	function<int ()> play_post_draw,
	string fragment_source
	)
	: _vsync_enabled(false)
	, _playing(false)
	, _draw_requested(false)
	, _setup_shaders(setup_shaders)
	, _create_textures(create_textures)
	, _play_pre_draw(play_pre_draw)
	, _draw(draw)
	, _play_post_draw(play_post_draw)
	, _fragment_source(std::move(fragment_source))
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
	_canvas->Bind(wxEVT_PAINT, boost::bind(&GLView::request_draw, this));
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
	Sized();
}


wxSize
GLView::size() const
{
	return _canvas_size.load();
}


static constexpr char vertex_source[] =
"#version 330 core\n"
"\n"
"layout (location = 0) in vec3 in_pos;\n"
"layout (location = 1) in vec2 in_tex_coord;\n"
"\n"
"out vec2 TexCoord;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position = vec4(in_pos, 1.0);\n"
"	TexCoord = in_tex_coord;\n"
"}\n";


void
GLView::setup_shaders()
{
	if (!_setup_shaders_done) {
		DCPOMATIC_ASSERT(_canvas);
		DCPOMATIC_ASSERT(_context);
		auto r = _canvas->SetCurrent(*_context);
		DCPOMATIC_ASSERT(r);

#ifdef DCPOMATIC_WINDOWS
		r = glewInit();
		if (r != GLEW_OK) {
			boost::throw_exception(GLError(reinterpret_cast<char const*>(glewGetErrorString(r))));
		}
#endif

		glGenVertexArrays(1, &_vao);
		check_gl_error("glGenVertexArrays");
		GLuint vbo;
		glGenBuffers(1, &vbo);
		check_gl_error("glGenBuffers");
		GLuint ebo;
		glGenBuffers(1, &ebo);
		check_gl_error("glGenBuffers");

		glBindVertexArray(_vao);
		check_gl_error("glBindVertexArray");

		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		check_gl_error("glBindBuffer");

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		check_gl_error("glBindBuffer");

		/* position attribute to vertex shader (location = 0) */
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
		glEnableVertexAttribArray(0);
		/* texture coord attribute to vertex shader (location = 1) */
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		check_gl_error("glEnableVertexAttribArray");

		auto vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
		auto fragment_shader = compile_shader(GL_FRAGMENT_SHADER, _fragment_source.c_str());

		_program = glCreateProgram();
		check_gl_error("glCreateProgram");
		glAttachShader(_program, vertex_shader);
		check_gl_error("glAttachShader");
		glAttachShader(_program, fragment_shader);
		check_gl_error("glAttachShader");
		glLinkProgram(_program);
		check_gl_error("glLinkProgram");
		GLint ok;
		glGetProgramiv(_program, GL_LINK_STATUS, &ok);
		if (!ok) {
			GLint log_length;
			glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &log_length);
			string log;
			if (log_length > 0) {
				std::vector<char> log_char(log_length);
				glGetProgramInfoLog(_program, log_length, nullptr, log_char.data());
				log = string(log_char.data());
			}
			glDeleteProgram(_program);
			boost::throw_exception(GLError(String::compose("Could not link shader (%1)", log).c_str(), -1));
		}
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);

		glUseProgram(_program);

		_setup_shaders();
		_setup_shaders_done = true;
	}
}


void
GLView::request_draw()
{
	if (!_canvas->IsShownOnScreen()) {
		return;
	}

	/* It appears important to do this from the UI thread; if we do it from the GL thread
	 * on Linux we get strange failures to create the context for any version of GL higher
	 * than 3.2.
	 */
	ensure_context();

#ifdef DCPOMATIC_OSX
	/* macOS gives errors if we don't do this (and therefore [NSOpenGLContext setView:]) from the main thread */
	setup_shaders();
#endif

	if (!_thread.joinable()) {
		_thread = boost::thread(boost::bind(&GLView::thread, this));
	}

	{
		boost::mutex::scoped_lock lm(_playing_mutex);
		_draw_requested = true;
		_thread_work_condition.notify_all();
	}

	rethrow();
}


void
GLView::thread()
try
{
	start_of_thread("GLView");

#if defined(DCPOMATIC_OSX)
	/* Without this we see errors like
	 * ../src/osx/cocoa/glcanvas.mm(194): assert ""context"" failed in SwapBuffers(): should have current context [in thread 700006970000]
	 */
	WXGLSetCurrentContext(_context->GetWXGLContext());
#else
	setup_shaders();
#endif

#if defined(DCPOMATIC_LINUX) && defined(DCPOMATIC_HAVE_GLX_SWAP_INTERVAL_EXT)
	if (_canvas->IsExtensionSupported("GLX_EXT_swap_control")) {
		/* Enable vsync */
		auto dpy = wxGetX11Display();
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

	_create_textures();

	while (true) {
		boost::mutex::scoped_lock lm(_playing_mutex);
		while (!_playing && !_draw_requested) {
			_thread_work_condition.wait(lm);
		}
		lm.unlock();

		if (_playing) {
			_play_pre_draw();
			draw();
			_canvas->SwapBuffers();
			auto const sleep_ms = _play_post_draw();
			boost::this_thread::sleep(boost::posix_time::milliseconds(sleep_ms));
		} else if (_draw_requested) {
			_draw_requested = false;
			draw();
			_canvas->SwapBuffers();
		}
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
GLView::draw()
{
	auto const s = size();
	int const width = s.GetWidth();
	int const height = s.GetHeight();

	if (width < 64 || height < 0) {
		return;
	}

	glViewport(0, 0, width, height);
	check_gl_error("glViewport");

	glBindVertexArray(_vao);
	check_gl_error("glBindVertexArray");

	_draw();

	glFlush();
	check_gl_error("glFlush");
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


void
GLView::start()
{
	{
		boost::mutex::scoped_lock lm(_playing_mutex);
		_playing = true;
		_thread_work_condition.notify_all();
	}

	request_draw();
}


void
GLView::stop()
{
	boost::mutex::scoped_lock lm(_playing_mutex);
	_playing = false;
}


#endif


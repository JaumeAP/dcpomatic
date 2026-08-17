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


#include "gl_util.h"
#include "gl_view.h"
#include "lib/dcpomatic_log.h"
#include "lib/exceptions.h"
#include "lib/util.h"
#include <boost/exception/all.hpp>
#include <boost/thread.hpp>


#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::gl;


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


GLView::GLView(
	wxWindow* parent,
	char const* fragment_source,
	std::function<void ()> setup_shaders,
	std::function<void ()> setup_textures,
	std::function<void ()> thread_body,
	std::function<void ()> update
	)
	: _canvas_size(wxSize{})
	, _fragment_source(fragment_source)
	, _setup_shaders(setup_shaders)
	, _setup_textures(setup_textures)
	, _thread_body(thread_body)
	, _update(update)
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
	_canvas->Bind(wxEVT_PAINT, boost::bind(&GLView::update, this));
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
	LOG_GENERAL("GLView canvas size changed to {}x{}", width, height);
	Sized();
}


void
GLView::ensure_shaders()
{
	if (_setup_shaders_done) {
		return;
	}

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
	check_error("glGenVertexArrays");
	GLuint vbo;
	glGenBuffers(1, &vbo);
	check_error("glGenBuffers");
	GLuint ebo;
	glGenBuffers(1, &ebo);
	check_error("glGenBuffers");

	glBindVertexArray(_vao);
	check_error("glBindVertexArray");

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	check_error("glBindBuffer");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	check_error("glBindBuffer");

	/* position attribute to vertex shader (location = 0) */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
	glEnableVertexAttribArray(0);
	/* texture coord attribute to vertex shader (location = 1) */
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	check_error("glEnableVertexAttribArray");

	auto vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
	auto fragment_shader = compile_shader(GL_FRAGMENT_SHADER, _fragment_source.c_str());

	_program = glCreateProgram();
	check_error("glCreateProgram");
	glAttachShader(_program, vertex_shader);
	check_error("glAttachShader");
	glAttachShader(_program, fragment_shader);
	check_error("glAttachShader");
	glLinkProgram(_program);
	check_error("glLinkProgram");
	GLint ok;
	glGetProgramiv(_program, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLint log_length;
		glGetProgramiv(_program, GL_INFO_LOG_LENGTH, &log_length);
		std::string log;
		if (log_length > 0) {
			std::vector<char> log_char(log_length);
			glGetProgramInfoLog(_program, log_length, nullptr, log_char.data());
			log = std::string(log_char.data());
		}
		glDeleteProgram(_program);
		boost::throw_exception(GLError(fmt::format("Could not link shader ({})", log).c_str(), -1));
	}
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	glUseProgram(_program);

	_setup_shaders();
	_setup_shaders_done = true;
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
	WXGLSetCurrentContext(_view.context()->GetWXGLContext());
#else
	ensure_shaders();
#endif

#if defined(DCPOMATIC_LINUX) && defined(DCPOMATIC_HAVE_GLX_SWAP_INTERVAL_EXT)
	if (_view.get()->IsExtensionSupported("GLX_EXT_swap_control")) {
		/* Enable vsync */
		Display* dpy = wxGetX11Display();
		glXSwapIntervalEXT(dpy, DefaultScreen(dpy), 1);
		_vsync_enabled = true;
	}

#endif

#ifdef DCPOMATIC_WINDOWS
	if (_view.get()->IsExtensionSupported("WGL_EXT_swap_control")) {
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

	_setup_textures();

	while (true) {
		_thread_body();
	}

	/* XXX: leaks _context, but that seems preferable to deleting it here
	 * without also deleting the wxGLCanvas.
	 */
}
catch (boost::thread_interrupted&)
{

}
catch (...)
{
	store_current();
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
	if (!_context) {
		wxGLContextAttrs attrs;
		attrs.PlatformDefaults().CoreProfile().OGLVersion(4, 1).EndList();
		_context = new wxGLContext(_canvas, nullptr, &attrs);
		if (!_context->IsOK()) {
			boost::throw_exception(GLError("Making GL context", -1));
		}
	}

#ifdef DCPOMATIC_OSX
	/* macOS gives errors if we don't do this (and therefore [NSOpenGLContext setView:]) from the main thread */
	ensure_shaders();
#endif

	if (!_thread.joinable()) {
		_thread = boost::thread(boost::bind(&GLView::thread, this));
	}

	_update();

	rethrow();
}


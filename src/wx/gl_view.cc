/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

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

#include "gl_view.h"
#include "lib/image.h"
#include "lib/dcpomatic_assert.h"
#include <boost/bind.hpp>
#include <iostream>

#ifdef DCPOMATIC_OSX
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#else
#include <GL/glu.h>
#include <GL/glext.h>
#endif

using std::cout;
using boost::shared_ptr;

GLView::GLView (wxWindow *parent)
	: wxGLCanvas (parent, wxID_ANY, 0, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
	_context = new wxGLContext (this);
	Bind (wxEVT_PAINT, boost::bind(&GLView::paint, this, _1));

	glGenTextures (1, &_id);
	glBindTexture (GL_TEXTURE_2D, _id);
	glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
}

GLView::~GLView ()
{
	glDeleteTextures (1, &_id);
	delete _context;
}

static void
check_gl_error (char const * last)
{
	GLenum const e = glGetError ();
	if (e != GL_NO_ERROR) {
		cout << last << " failed " << e << " (";
		if (e == GL_INVALID_VALUE) {
			cout << "invalid value";
		} else if (e == GL_INVALID_ENUM) {
			cout << "invalid enum";
		} else {
			cout << "unknown";
		}
		cout << ")\n";
	}
}

void
GLView::paint (wxPaintEvent &)
{
	SetCurrent (*_context);
	wxPaintDC (this);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	check_gl_error ("glClear");

	glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_COLOR_MATERIAL);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glViewport (0, 0, GetSize().x, GetSize().y);
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	gluOrtho2D (0, GetSize().x, GetSize().y, 0);
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glTranslatef (0, 0, 0);

	if (_size) {
		glBegin (GL_QUADS);

		glTexCoord2f (0, 1);
		glVertex2f (0, _size->height);
		glTexCoord2f (1, 1);
		glVertex2f (_size->width, _size->height);
		glTexCoord2f (1, 0);
		glVertex2f (_size->width, 0);
		glTexCoord2f (0, 0);
		glVertex2f (0, 0);

		glEnd ();
	}

	glFlush();
	SwapBuffers();
}

void
GLView::set_image (shared_ptr<const Image> image)
{
	DCPOMATIC_ASSERT (image->pixel_format() == AV_PIX_FMT_RGB24);
	DCPOMATIC_ASSERT (!image->aligned());

	_size = image->size ();
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB8, image->size().width, image->size().height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data()[0]);

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	Refresh ();
}

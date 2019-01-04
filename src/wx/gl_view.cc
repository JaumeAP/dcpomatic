/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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
#include <boost/bind.hpp>
#include <iostream>

using std::cout;
using boost::shared_ptr;

GLView::GLView (wxWindow *parent)
	: wxGLCanvas (parent, wxID_ANY, 0)
{
	_context = new wxGLContext (this);
	Bind (wxEVT_PAINT, boost::bind(&GLView::paint, this, _1));
	Bind (wxEVT_SIZE, boost::bind(&GLView::sized, this));
}

GLView::~GLView ()
{
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
GLView::sized ()
{
	glViewport (0, 0, GetSize().x, GetSize().y);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho (0, GetSize().x, GetSize().y, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void
GLView::paint (wxPaintEvent &)
{
	SetCurrent (*_context);
	wxPaintDC (this);
	glClear (GL_COLOR_BUFFER_BIT);

	glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_DEPTH_TEST);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	int const width = GetSize().x;
	int const height = GetSize().y;

	glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_gl_error ("glTexParameteri");

	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1, 1, 1);
	glBindTexture(GL_TEXTURE_2D, tex);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2i(0, 0);
	glVertex2i(0, 0);
	glTexCoord2i(0, 1);
	glVertex2i(0, height);
	glTexCoord2i(1, 1);
	glVertex2i(width, height);
	glTexCoord2i(1, 0);
	glVertex2i(width, 0);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFlush();
	SwapBuffers();
}

void
GLView::set_image (shared_ptr<const Image> image)
{
	if (image->size() != _texture_size) {
		glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, image->size().width, image->size().height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
		check_gl_error ("glTexImage2D");
		glBindTexture(GL_TEXTURE_2D, 0);
		check_gl_error ("glBindTexture #1");
		_texture_size = image->size();
	}

	int const width = image->size().width;
	int const height = image->size().height;
	unsigned char foo[width * height * 3];
	unsigned char* p = foo;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (((y / 16) % 3) == 0) {
				*p++ = char(x * 256 / width);
			} else {
				*p++ = 0;
			}
			if (((y / 16) % 3) == 1) {
				*p++ = char(x * 256 / width);
			} else {
				*p++ = 0;
			}
			if (((y / 16) % 3) == 2) {
				*p++ = char(x * 256 / width);
			} else {
				*p++ = 0;
			}
		}
	}

	glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, _texture_size.width, _texture_size.height, GL_RGB, GL_UNSIGNED_BYTE, foo);//image->data()[0]);
	check_gl_error ("glTexSubImage2D");
	glBindTexture(GL_TEXTURE_2D, 0);
	check_gl_error ("glBindTexture2D #2");

	glFlush();
	if (GetXWindow()) {
		SwapBuffers();
	}
}

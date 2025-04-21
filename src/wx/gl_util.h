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


#include <dcp/types.h>
#include <wx/wx.h>


/* This will only build on an new-enough wxWidgets: see the comment in gl_video_view.h */
#if wxCHECK_VERSION(3,1,0)

#ifdef DCPOMATIC_OSX
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#endif

#ifdef DCPOMATIC_LINUX
#include <GL/glu.h>
#include <GL/glext.h>
#endif

#ifdef DCPOMATIC_WINDOWS
#include <GL/glu.h>
#include <GL/wglext.h>
#endif

extern GLuint compile_shader(GLenum type, char const* source);
extern void check_gl_error(char const* last);


namespace dcpomatic {
namespace gl {

class Rectangle
{
public:
	Rectangle(wxSize canvas_size, float x, float y, dcp::Size size)
		: _canvas_size(canvas_size)
	{
		auto const x1 = x_pixels_to_gl(x);
		auto const y1 = y_pixels_to_gl(y);
		auto const x2 = x_pixels_to_gl(x + size.width);
		auto const y2 = y_pixels_to_gl(y + size.height);

		/* The texture coordinates here have to account for the fact that when we put images into the texture OpenGL
		 * expected us to start at the lower left but we actually started at the top left.  So although the
		 * top of the texture is at 1.0 we pretend it's the other way round.
		 */

		// bottom right
		_vertices[0] = x2;
		_vertices[1] = y2;
		_vertices[2] = 0.0f;
		_vertices[3] = 1.0f;
		_vertices[4] = 1.0f;

		// top right
		_vertices[5] = x2;
		_vertices[6] = y1;
		_vertices[7] = 0.0f;
		_vertices[8] = 1.0f;
		_vertices[9] = 0.0f;

		// top left
		_vertices[10] = x1;
		_vertices[11] = y1;
		_vertices[12] = 0.0f;
		_vertices[13] = 0.0f;
		_vertices[14] = 0.0f;

		// bottom left
		_vertices[15] = x1;
		_vertices[16] = y2;
		_vertices[17] = 0.0f;
		_vertices[18] = 0.0f;
		_vertices[19] = 1.0f;
	}

	float const * vertices() const {
		return _vertices;
	}

	/** @return size of data returned from vertices() in bytes */
	int const size() const {
		return sizeof(_vertices);
	}

private:
	/* @param x x position in pixels where 0 is left and canvas_width is right on screen */
	float x_pixels_to_gl(int x) const {
		return (x * 2.0f / _canvas_size.GetWidth()) - 1.0f;
	}

	/* @param y y position in pixels where 0 is top and canvas_height is bottom on screen */
	float y_pixels_to_gl(int y) const {
		return 1.0f - (y * 2.0f / _canvas_size.GetHeight());
	}

	wxSize _canvas_size;
	float _vertices[20];
};


}
}


#endif


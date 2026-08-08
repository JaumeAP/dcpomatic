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


#ifndef DCPOMATIC_GL_UTIL_H
#define DCPOMATIC_GL_UTIL_H


#include "lib/crop.h"
#include "lib/image.h"
#include <dcp/types.h>
#include <wx/wx.h>


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


namespace dcpomatic {
namespace gl {


extern void check_error(char const * last);
extern GLuint compile_shader(GLenum type, char const* source);


class Uniform
{
public:
	Uniform() = default;
	Uniform(int program, char const* name);

	void setup(int program, char const* name);

protected:
	int _location = -1;
};


class UniformVec4f : public Uniform
{
public:
	UniformVec4f() = default;
	UniformVec4f(int program, char const* name);

	void set(float a, float b, float c, float d);
};


class Uniform1i : public Uniform
{
public:
	Uniform1i() = default;
	Uniform1i(int program, char const* name);

	void set(int v);
};


class UniformMatrix4fv : public Uniform
{
public:
	UniformMatrix4fv() = default;
	UniformMatrix4fv(int program, char const* name);

	void set(float const* matrix);
};


class Rectangle
{
public:
	Rectangle(wxSize canvas_size, float x, float y, dcp::Size out_size, Crop crop, dcp::Size original_size)
		: _canvas_size(canvas_size)
	{
		auto const x1 = x_pixels_to_gl(x);
		auto const y1 = y_pixels_to_gl(y);
		auto const x2 = x_pixels_to_gl(x + out_size.width);
		auto const y2 = y_pixels_to_gl(y + out_size.height);

		/* The texture coordinates here have to account for the fact that when we put images into the texture OpenGL
		 * expected us to start at the lower left but we actually started at the top left.  So although the
		 * top of the texture is at 1.0 we pretend it's the other way round.
		 */

		float const top = static_cast<float>(crop.top) / original_size.height;
		float const bottom = 1.0f - (static_cast<float>(crop.bottom) / original_size.height);
		float const left = static_cast<float>(crop.left) / original_size.width;
		float const right = 1.0f - static_cast<float>(crop.right) / original_size.width;

		// bottom right
		_vertices[0] = x2;
		_vertices[1] = y2;
		_vertices[2] = 0.0f;
		_vertices[3] = right;
		_vertices[4] = bottom;

		// top right
		_vertices[5] = x2;
		_vertices[6] = y1;
		_vertices[7] = 0.0f;
		_vertices[8] = right;
		_vertices[9] = top;

		// top left
		_vertices[10] = x1;
		_vertices[11] = y1;
		_vertices[12] = 0.0f;
		_vertices[13] = left;
		_vertices[14] = top;

		// bottom left
		_vertices[15] = x1;
		_vertices[16] = y2;
		_vertices[17] = 0.0f;
		_vertices[18] = left;
		_vertices[19] = bottom;
	}

	float const * vertices() const {
		return _vertices;
	}

	int const size() const {
		return sizeof(_vertices);
	}

private:
	/* @param x x position in pixels where 0 is left and canvas_width is right on screen */
	float x_pixels_to_gl(int x) const {
		return x * 2.0f / _canvas_size.GetWidth() - 1.0f;
	}

	/* @param y y position in pixels where 0 is top and canvas_height is bottom on screen */
	float y_pixels_to_gl(int y) const {
		return 1.0f - y * 2.0f / _canvas_size.GetHeight();
	}

	wxSize _canvas_size;
	float _vertices[20];
};


class Texture
{
public:
	/** @param unit Unit, or index, of this texture (from 0 to GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS - 1) */
	Texture(GLint unpack_alignment, int unit = 0);
	~Texture();

	Texture(Texture&&);
	Texture& operator=(Texture&&);

	Texture(Texture const&) = delete;
	Texture& operator=(Texture const&) = delete;

	void bind();
	void set(std::shared_ptr<const Image> image, int component = 0);

private:
	GLuint _name;
	GLint _unpack_alignment;
	int _unit;
	boost::optional<dcp::Size> _size;
};


}
}


#endif


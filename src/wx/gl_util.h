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


#include "lib/dcpomatic_assert.h"
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


class Image;


namespace dcpomatic {
namespace gl {


extern GLuint compile_shader(GLenum type, char const* source);
extern void check_gl_error(char const* last);


class Texture
{
public:
	/** @param unit Unit, or index, of this texture (from 0 to GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS - 1) */
	Texture(GLint unpack_alignment, int unit = 0);
	~Texture();

	Texture(Texture const&) = delete;
	Texture& operator=(Texture const&) = delete;

	Texture(Texture&&);
	Texture& operator=(Texture&&);

	void bind();
	void set(std::shared_ptr<const Image> image, int component = 0);

private:
	GLuint _name;
	GLint _unpack_alignment;
	int _unit;
	boost::optional<dcp::Size> _size;
};


class Coords
{
public:
	class Rectangle
	{
	public:
		Rectangle(Coords& coords, int node, int index);

		virtual void draw() = 0;

		void set(wxSize canvas_size, int x, int y, dcp::Size size);
		void set(wxSize canvas_size, int x, int y, wxSize size);

		int index() const {
			return _index;
		}

	protected:
		Coords& _coords;
		int _node;
		int _index;
	};

	class FilledRectangle : public Rectangle
	{
	public:
		FilledRectangle(Coords& coords);
		void draw() override;
	};

	class OutlineRectangle : public Rectangle
	{
	public:
		OutlineRectangle(Coords& coords);
		void draw() override;
	};

	class Line
	{
	public:
		Line(Coords& coords);

		void set(wxSize canvas_size, int x1, int y1, int x2, int y2);
		void draw();

	private:
		Coords& _coords;
		int _node;
		int _index;
	};

	class FilledRectangleGroup
	{
	public:
		void add(FilledRectangle const& rectangle);
		void draw();

	private:
		int _index = -1;
		int _number = 0;
	};

	FilledRectangle add_filled_rectangle();
	OutlineRectangle add_outline_rectangle();
	Line add_line();

	void setup();
	void set_array_buffer();

private:
	friend class Rectangle;

	bool _setup = false;
	int _nodes = 0;
	std::vector<unsigned int> _indices;
	std::vector<float> _coords;
};


class Uniform
{
public:
	void setup(int program, char const* name);

protected:
	int _location = -1;
};


class UniformVec4f : public Uniform
{
public:
	void set(float a, float b, float c, float d);
};


class Uniform1i : public Uniform
{
public:
	void set(int v);
};


}
}


#endif
#endif


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


class Coords
{
public:
	class Rectangle
	{
	public:
		Rectangle(Coords& coords, int node, int index);

		virtual void draw() = 0;

		void set(wxSize canvas_size, int x, int y, dcp::Size size, Crop crop = {}, dcp::Size original_size = {1, 1});
		void set(wxSize canvas_size, int x, int y, wxSize size, Crop crop = {}, wxSize original_size = {1, 1});

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

	void setup();
	void set_array_buffer();

private:
	friend class Rectangle;

	bool _setup = false;
	int _nodes = 0;
	std::vector<unsigned int> _indices;
	std::vector<float> _coords;
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


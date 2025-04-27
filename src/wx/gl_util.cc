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
#include "lib/compose.hpp"
#include "lib/dcpomatic_assert.h"
#include "lib/image.h"
#include <wx/wx.h>


using std::shared_ptr;
using std::string;
using namespace dcpomatic::gl;


/* This will only build on an new-enough wxWidgets: see the comment in gl_video_view.h */
#if wxCHECK_VERSION(3,1,0)

GLuint
dcpomatic::gl::compile_shader(GLenum type, char const* source)
{
	auto shader = glCreateShader(type);
	DCPOMATIC_ASSERT(shader);
	GLchar const * src[] = { static_cast<GLchar const *>(source) };
	glShaderSource(shader, 1, src, nullptr);
	check_gl_error("glShaderSource");
	glCompileShader(shader);
	check_gl_error("glCompileShader");
	GLint ok;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLint log_length;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
		string log;
		if (log_length > 0) {
			std::vector<char> log_char(log_length);
			glGetShaderInfoLog(shader, log_length, nullptr, log_char.data());
			log = string(log_char.data());
		}
		glDeleteShader(shader);
		boost::throw_exception(GLError(String::compose("Could not compile shader (%1)", log).c_str(), -1));
	}
	return shader;
}


void
dcpomatic::gl::check_gl_error(char const * last)
{
	GLenum const e = glGetError();
	if (e != GL_NO_ERROR) {
		boost::throw_exception(GLError(last, e));
	}
}


Texture::Texture(GLint unpack_alignment, int unit)
	: _unpack_alignment(unpack_alignment)
	, _unit(unit)
{
	glGenTextures(1, &_name);
	check_gl_error("glGenTextures");
}


Texture::Texture(Texture&& other)
	: _name(other._name)
	, _unpack_alignment(other._unpack_alignment)
	, _unit(other._unit)
	, _size(std::move(other._size))
{
	other._name = 0;
}


Texture&
Texture::operator=(Texture&& other)
{
	if (this == &other) {
		return *this;
	}

	_name = other._name;
	_unpack_alignment = other._unpack_alignment;
	_unit = other._unit;
	_size = std::move(other._size);

	other._name = 0;

	return *this;
}


Texture::~Texture()
{
	glDeleteTextures(1, &_name);
}


void
Texture::bind()
{
	glActiveTexture(GL_TEXTURE0 + _unit);
	check_gl_error("glActiveTexture");
	glBindTexture(GL_TEXTURE_2D, _name);
	check_gl_error("glBindTexture");
}


void
Texture::set(shared_ptr<const Image> image, int component)
{
	auto const create = !_size || image->size() != _size;
	_size = image->size();

	glPixelStorei(GL_UNPACK_ALIGNMENT, _unpack_alignment);
	check_gl_error("glPixelStorei");

	DCPOMATIC_ASSERT(image->alignment() == Image::Alignment::COMPACT);

	GLint internal_format;
	GLenum format;
	GLenum type;
	int subsample = 1;

	switch (image->pixel_format()) {
	case AV_PIX_FMT_YUV420P:
		internal_format = GL_R8;
		format = GL_RED;
		type = GL_UNSIGNED_BYTE;
		subsample = component > 0 ? 2 : 1;
		break;
	case AV_PIX_FMT_BGRA:
		internal_format = GL_RGBA8;
		format = GL_BGRA;
		type = GL_UNSIGNED_BYTE;
		break;
	case AV_PIX_FMT_RGBA:
		internal_format = GL_RGBA8;
		format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case AV_PIX_FMT_RGB24:
		internal_format = GL_RGBA8;
		format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case AV_PIX_FMT_XYZ12:
		internal_format = GL_RGBA12;
		format = GL_RGB;
		type = GL_UNSIGNED_SHORT;
		break;
	default:
		boost::throw_exception(PixelFormatError("Texture::set", image->pixel_format()));
	}

	bind();

	if (create) {
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, _size->width / subsample, _size->height / subsample, 0, format, type, image->data()[component]);
		check_gl_error("glTexImage2D");
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _size->width / subsample, _size->height / subsample, format, type, image->data()[component]);
		check_gl_error("glTexSubImage2D");
	}
}


/** @param x x position in pixels where 0 is left and canvas_width is right on screen */
static
float
x_pixels_to_gl(wxSize const& canvas_size, int x) {
	return (x * 2.0f / canvas_size.GetWidth()) - 1.0f;
}


/** @param y y position in pixels where 0 is top and canvas_height is bottom on screen */
static
float
y_pixels_to_gl(wxSize const& canvas_size, int y) {
	return 1.0f - (y * 2.0f / canvas_size.GetHeight());
}


Coords::Rectangle::Rectangle(Coords& coords, int node, int index)
	: _coords(coords)
	, _node(node)
	, _index(index)
{
	auto& c = _coords._coords;

	auto offset = c.size();

	c.resize(c.size() + 5 * 4);

	c[offset + 2] = 0.0f;
	c[offset + 3] = 1.0f;
	c[offset + 4] = 1.0f;

	c[offset + 7] = 0.0f;
	c[offset + 8] = 1.0f;
	c[offset + 9] = 0.0f;

	c[offset + 12] = 0.0f;
	c[offset + 13] = 0.0f;
	c[offset + 14] = 0.0f;

	c[offset + 17] = 0.0f;
	c[offset + 18] = 0.0f;
	c[offset + 19] = 1.0f;
}


void
Coords::Rectangle::set(wxSize canvas_size, int x, int y, dcp::Size size)
{
	set(canvas_size, x, y, wxSize(size.width, size.height));
}


void
Coords::Rectangle::set(wxSize canvas_size, int x, int y, wxSize size)
{
	auto const x1 = x_pixels_to_gl(canvas_size, x);
	auto const y1 = y_pixels_to_gl(canvas_size, y);
	auto const x2 = x_pixels_to_gl(canvas_size, x + size.GetWidth());
	auto const y2 = y_pixels_to_gl(canvas_size, y + size.GetHeight());

	/* The texture coordinates here have to account for the fact that when we put images into the texture OpenGL
	 * expected us to start at the lower left but we actually started at the top left.  So although the
	 * top of the texture is at 1.0 we pretend it's the other way round.
	 */

	auto& coords = _coords._coords;

	auto const o = _node * 5;
	DCPOMATIC_ASSERT((o + 16) < static_cast<int>(coords.size()));

	// bottom right
	coords[o] = x2;
	coords[o + 1] = y2;

	// top right
	coords[o + 5] = x2;
	coords[o + 6] = y1;

	// top left
	coords[o + 10] = x1;
	coords[o + 11] = y1;

	// bottom left
	coords[o + 15] = x1;
	coords[o + 16] = y2;
}


Coords::FilledRectangle::FilledRectangle(Coords& coords)
	: Rectangle(coords, coords._nodes, coords._indices.size())
{
	auto& indices = coords._indices;
	auto& nodes = coords._nodes;

	indices.push_back(nodes + 0);
	indices.push_back(nodes + 1);
	indices.push_back(nodes + 3);
	indices.push_back(nodes + 1);
	indices.push_back(nodes + 2);
	indices.push_back(nodes + 3);
	nodes += 4;
}


void
Coords::FilledRectangle::draw()
{
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(_index * sizeof(int)));
	check_gl_error("glDrawElements");
}


Coords::FilledRectangle
Coords::add_filled_rectangle()
{
	DCPOMATIC_ASSERT(!_setup);
	return FilledRectangle(*this);
}


Coords::OutlineRectangle::OutlineRectangle(Coords& coords)
	: Rectangle(coords, coords._nodes, coords._indices.size())
{
	auto& indices = _coords._indices;
	auto& nodes = _coords._nodes;

	indices.push_back(nodes + 0);
	indices.push_back(nodes + 1);
	indices.push_back(nodes + 1);
	indices.push_back(nodes + 2);
	indices.push_back(nodes + 2);
	indices.push_back(nodes + 3);
	indices.push_back(nodes + 3);
	indices.push_back(nodes + 0);
	nodes += 4;
}


void
Coords::OutlineRectangle::draw()
{
	glDrawElements(GL_LINES, 8, GL_UNSIGNED_INT, reinterpret_cast<void*>(_index * sizeof(int)));
	check_gl_error("glDrawElements");
}


Coords::OutlineRectangle
Coords::add_outline_rectangle()
{
	DCPOMATIC_ASSERT(!_setup);
	return OutlineRectangle(*this);
}


void
Coords::FilledRectangleGroup::add(Coords::FilledRectangle const& rectangle)
{
	if (_index == -1) {
		_index = rectangle.index();
	} else {
		DCPOMATIC_ASSERT(rectangle.index() == _index + _number);
	}
	_number += 6;
}


void
Coords::FilledRectangleGroup::draw()
{
	glDrawElements(GL_TRIANGLES, _number, GL_UNSIGNED_INT, reinterpret_cast<void*>(_index * sizeof(int)));
	check_gl_error("glDrawElements");
}


void
Coords::setup()
{
	DCPOMATIC_ASSERT(!_setup);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, _indices.size() * sizeof(unsigned int), _indices.data(), GL_STATIC_DRAW);
	check_gl_error("glBufferData");
	glBufferData(GL_ARRAY_BUFFER, _nodes * 5 * sizeof(float), nullptr, GL_STATIC_DRAW);
	check_gl_error("glBufferData");

	_setup = true;
}


void
Coords::set_array_buffer()
{
	DCPOMATIC_ASSERT(_setup);
	glBufferSubData(GL_ARRAY_BUFFER, 0, _coords.size() * sizeof(float), _coords.data());
	check_gl_error("glBufferSubData");
}


#endif


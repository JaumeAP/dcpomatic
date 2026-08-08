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


#ifdef DCPOMATIC_WINDOWS
#include <GL/glew.h>
#endif

#include "gl_util.h"
#include "lib/dcpomatic_assert.h"

#include <wx/version.h>
/* This will only build on an new-enough wxWidgets: see the comment in gl_util.h */
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


using std::shared_ptr;
using std::string;
using namespace dcpomatic::gl;


void
dcpomatic::gl::check_error(char const * last)
{
	auto const e = glGetError();
	if (e != GL_NO_ERROR) {
		boost::throw_exception(GLError(last, e));
	}
}


Uniform::Uniform(int program, char const* name)
{
	setup(program, name);
}


void
Uniform::setup(int program, char const* name)
{
       _location = glGetUniformLocation(program, name);
       check_error("glGetUniformLocation");
}


UniformVec4f::UniformVec4f(int program, char const* name)
	: Uniform(program, name)
{

}


void
UniformVec4f::set(float a, float b, float c, float d)
{
       DCPOMATIC_ASSERT(_location != -1);
       glUniform4f(_location, a, b, c, d);
       check_error("glUniform4f");
}


Uniform1i::Uniform1i(int program, char const* name)
	: Uniform(program, name)
{

}


void
Uniform1i::set(int v)
{
       DCPOMATIC_ASSERT(_location != -1);
       glUniform1i(_location, v);
       check_error("glUniform1i");
}


UniformMatrix4fv::UniformMatrix4fv(int program, char const* name)
	: Uniform(program, name)
{

}


void
UniformMatrix4fv::set(float const* matrix)
{
       DCPOMATIC_ASSERT(_location != -1);
       glUniformMatrix4fv(_location, 1, GL_TRUE, matrix);
       check_error("glUniformMatrix4fv");
}


Texture::Texture(GLint unpack_alignment, int unit)
	: _unpack_alignment(unpack_alignment)
	, _unit(unit)
{
	glGenTextures(1, &_name);
	check_error("glGenTextures");
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
	check_error("glActiveTexture");
	glBindTexture(GL_TEXTURE_2D, _name);
	check_error("glBindTexture");
}


void
Texture::set(shared_ptr<const Image> image, int component)
{
	auto const create = !_size || image->size() != _size;
	_size = image->size();

	glPixelStorei(GL_UNPACK_ALIGNMENT, _unpack_alignment);
	check_error("glPixelStorei");

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
		check_error("glTexImage2D");
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _size->width / subsample, _size->height / subsample, format, type, image->data()[component]);
		check_error("glTexSubImage2D");
	}
}


GLuint
dcpomatic::gl::compile_shader(GLenum type, char const* source)
{
	auto shader = glCreateShader(type);
	DCPOMATIC_ASSERT(shader);
	GLchar const * src[] = { static_cast<GLchar const *>(source) };
	glShaderSource(shader, 1, src, nullptr);
	check_error("glShaderSource");
	glCompileShader(shader);
	check_error("glCompileShader");
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
		boost::throw_exception(GLError(fmt::format("Could not compile shader ({})", log).c_str(), -1));
	}
	return shader;
}



#endif


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
#include <wx/wx.h>


using std::string;


/* This will only build on an new-enough wxWidgets: see the comment in gl_video_view.h */
#if wxCHECK_VERSION(3,1,0)

GLuint
compile_shader(GLenum type, char const* source)
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
check_gl_error(char const * last)
{
	GLenum const e = glGetError();
	if (e != GL_NO_ERROR) {
		boost::throw_exception(GLError(last, e));
	}
}


#endif


/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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

#include "gl_video_view.h"

/* This will only build on an new-enough wxWidgets: see the comment in gl_video_view.h */
#if wxCHECK_VERSION(3,1,0)

#include "film_viewer.h"
#include "gl_util.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/cross.h"
#include "lib/dcpomatic_assert.h"
#include "lib/dcpomatic_log.h"
#include "lib/exceptions.h"
#include "lib/image.h"
#include "lib/player_video.h"
#include <boost/bind/bind.hpp>
#include <iostream>

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


using std::cout;
using std::shared_ptr;
using std::string;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/* Bicubic interpolation stolen from https://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl */
static constexpr char fragment_source[] =
"#version 330 core\n"
"\n"
"in vec2 TexCoord;\n"
"\n"
"uniform sampler2D texture_sampler_0;\n"
"uniform sampler2D texture_sampler_1;\n"
"uniform sampler2D texture_sampler_2;\n"
"uniform sampler2D texture_sampler_3;\n"
/* type = 0: draw outline content rectangle
 * type = 1: draw crop guess rectangle
 * type = 2: draw XYZ image from texture_sampler_0
 * type = 3: draw RGB image (with sRGB/Rec709 primaries) from texture_sampler_0
 * type = 4: draw RGB image (with sRGB/Rec709 primaries) from texture_sampler_3
 * type = 5: draw RGB image (converting from Rec2020 primaries) from texture_sampler_0
 * type = 6; draw YUV image (Y in texture_sampler_0, U in texture_sampler_1, V in texture_sampler_2)
 * See FragmentType enum below.
 */
"uniform int type = 0;\n"
"uniform vec4 outline_content_colour;\n"
"uniform vec4 crop_guess_colour;\n"
"uniform mat4 xyz_rec709_colour_conversion;\n"
"uniform mat4 rec2020_rec709_colour_conversion;\n"
"\n"
"out vec4 FragColor;\n"
"\n"
"vec4 cubic(float x)\n"
"\n"
"#define IN_GAMMA 2.6\n"
"#define OUT_GAMMA 0.454545455\n"       //  1 /  2.2
"#define DCI_COEFFICIENT 0.91655528\n"  // 48 / 53.37
"\n"
"{\n"
"    float x2 = x * x;\n"
"    float x3 = x2 * x;\n"
"    vec4 w;\n"
"    w.x =     -x3 + 3 * x2 - 3 * x + 1;\n"
"    w.y =  3 * x3 - 6 * x2         + 4;\n"
"    w.z = -3 * x3 + 3 * x2 + 3 * x + 1;\n"
"    w.w =  x3;\n"
"    return w / 6.f;\n"
"}\n"
"\n"
"vec4 texture_bicubic(sampler2D sampler, vec2 tex_coords)\n"
"{\n"
"   vec2 tex_size = textureSize(sampler, 0);\n"
"   vec2 inv_tex_size = 1.0 / tex_size;\n"
"\n"
"   tex_coords = tex_coords * tex_size - 0.5;\n"
"\n"
"   vec2 fxy = fract(tex_coords);\n"
"   tex_coords -= fxy;\n"
"\n"
"   vec4 xcubic = cubic(fxy.x);\n"
"   vec4 ycubic = cubic(fxy.y);\n"
"\n"
"   vec4 c = tex_coords.xxyy + vec2(-0.5, +1.5).xyxy;\n"
"\n"
"   vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);\n"
"   vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;\n"
"\n"
"   offset *= inv_tex_size.xxyy;\n"
"\n"
"   vec4 sample0 = texture(sampler, offset.xz);\n"
"   vec4 sample1 = texture(sampler, offset.yz);\n"
"   vec4 sample2 = texture(sampler, offset.xw);\n"
"   vec4 sample3 = texture(sampler, offset.yw);\n"
"\n"
"   float sx = s.x / (s.x + s.y);\n"
"   float sy = s.z / (s.z + s.w);\n"
"\n"
"   return mix(\n"
"	   mix(sample3, sample2, sx), mix(sample1, sample0, sx)\n"
"	   , sy);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	switch (type) {\n"
"		case 0:\n"
"			FragColor = outline_content_colour;\n"
"			break;\n"
"		case 1:\n"
"                       FragColor = crop_guess_colour;\n"
"                       break;\n"
"		case 2:\n"
"			FragColor = texture_bicubic(texture_sampler_0, TexCoord);\n"
"			FragColor.x = pow(FragColor.x, IN_GAMMA) / DCI_COEFFICIENT;\n"
"			FragColor.y = pow(FragColor.y, IN_GAMMA) / DCI_COEFFICIENT;\n"
"			FragColor.z = pow(FragColor.z, IN_GAMMA) / DCI_COEFFICIENT;\n"
"			FragColor = xyz_rec709_colour_conversion * FragColor;\n"
"			FragColor.x = pow(FragColor.x, OUT_GAMMA);\n"
"			FragColor.y = pow(FragColor.y, OUT_GAMMA);\n"
"			FragColor.z = pow(FragColor.z, OUT_GAMMA);\n"
"			break;\n"
"		case 3:\n"
"			FragColor = texture_bicubic(texture_sampler_0, TexCoord);\n"
"                       break;\n"
"		case 4:\n"
"			FragColor = texture_bicubic(texture_sampler_3, TexCoord);\n"
"                       break;\n"
"		case 5:\n"
"			FragColor = texture_bicubic(texture_sampler_0, TexCoord);\n"
"			FragColor = rec2020_rec709_colour_conversion * FragColor;\n"
"			break;\n"
"               case 6:\n"
"                       float y = texture_bicubic(texture_sampler_0, TexCoord).x;\n"
"                       float u = texture_bicubic(texture_sampler_1, TexCoord).x - 0.5;\n"
"                       float v = texture_bicubic(texture_sampler_2, TexCoord).x - 0.5;\n"
			// From https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion
"                       FragColor.x = y + 1.5748 * v;\n"
"                       FragColor.y = y - 0.1873 * u - 0.4681 * v;\n"
"                       FragColor.z = y + 1.8556 * u;\n"
"                       FragColor.a = 1;\n"
"                       break;\n"
"	}\n"
"}\n";


GLVideoView::GLVideoView(FilmViewer* viewer, wxWindow *parent)
	: VideoView(viewer)
	, _view(
		parent,
		boost::bind(&GLVideoView::setup_shaders, this),
		boost::bind(&GLVideoView::create_textures, this),
		boost::bind(&GLVideoView::play_pre_draw, this),
		boost::bind(&GLVideoView::draw, this),
		boost::bind(&GLVideoView::play_post_draw, this),
		fragment_source
	)
	, _rec2020(false)
	, _meters(parent, viewer)
{
	_view.Sized.connect(boost::ref(Sized));

	_view.canvas()->Bind(wxEVT_TIMER, boost::bind(&GLVideoView::check_for_butler_errors, this));
	_timer.reset(new wxTimer(_view.canvas()));
	_timer->Start(2000);
}


void
GLVideoView::check_for_butler_errors()
{
	if (!_viewer->butler()) {
		return;
	}

	try {
		_viewer->butler()->rethrow();
	} catch (DecodeError& e) {
		error_dialog(get(), std_to_wx(e.what()));
	} catch (dcp::ReadError& e) {
		error_dialog(get(), wxString::Format(_("Could not read DCP: %s"), std_to_wx(e.what())));
	}

	LOG_DEBUG_PLAYER("Latency %1", _viewer->average_latency());
}


enum class FragmentType
{
	OUTLINE_CONTENT = 0,
	CROP_GUESS = 1,
	XYZ_IMAGE = 2,
	REC709_IMAGE = 3,
	REC709_SUBTITLE = 4,
	REC2020_IMAGE = 5,
	YUV420P_IMAGE = 6,
};


/* Offset and number of indices for the things in the indices array below */
static constexpr int indices_video_texture_offset = 0;
static constexpr int indices_video_texture_number = 6;
static constexpr int indices_subtitle_texture_offset = indices_video_texture_offset + indices_video_texture_number;
static constexpr int indices_subtitle_texture_number = 6;
static constexpr int indices_outline_content_offset = indices_subtitle_texture_offset + indices_subtitle_texture_number;
static constexpr int indices_outline_content_number = 8;
static constexpr int indices_crop_guess_offset = indices_outline_content_offset + indices_outline_content_number;
static constexpr int indices_crop_guess_number = 8;

static constexpr unsigned int indices[] = {
	0, 1, 3, // video texture triangle #1
	1, 2, 3, // video texture triangle #2
	4, 5, 7, // subtitle texture triangle #1
	5, 6, 7, // subtitle texture triangle #2
	8, 9,    // outline content line #1
	9, 10,   // outline content line #2
	10, 11,  // outline content line #3
	11, 8,   // outline content line #4
	12, 13,  // crop guess line #1
	13, 14,  // crop guess line #2
	14, 15,  // crop guess line #3
	15, 12,  // crop guess line #4
};

/* Offsets of things in the GL_ARRAY_BUFFER */
static constexpr int array_buffer_video_offset = 0;
static constexpr int array_buffer_subtitle_offset = array_buffer_video_offset + 4 * 5 * sizeof(float);
static constexpr int array_buffer_outline_content_offset = array_buffer_subtitle_offset + 4 * 5 * sizeof(float);
static constexpr int array_buffer_crop_guess_offset = array_buffer_outline_content_offset + 4 * 5 * sizeof(float);


void
GLVideoView::setup_shaders()
{
	auto get_information = [this](GLenum name) {
		auto s = glGetString(name);
		if (s) {
			_information[name] = std::string(reinterpret_cast<char const *>(s));
		}
	};

	get_information(GL_VENDOR);
	get_information(GL_RENDERER);
	get_information(GL_VERSION);
	get_information(GL_SHADING_LANGUAGE_VERSION);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	check_gl_error("glBufferData");

	auto texture_0 = glGetUniformLocation(_view.program(), "texture_sampler_0");
	check_gl_error("glGetUniformLocation");
	glUniform1i(texture_0, 0);
	check_gl_error("glUniform1i");
	auto texture_1 = glGetUniformLocation(_view.program(), "texture_sampler_1");
	check_gl_error("glGetUniformLocation");
	glUniform1i(texture_1, 1);
	check_gl_error("glUniform1i");
	auto texture_2 = glGetUniformLocation(_view.program(), "texture_sampler_2");
	check_gl_error("glGetUniformLocation");
	glUniform1i(texture_2, 2);
	check_gl_error("glUniform1i");
	auto texture_3 = glGetUniformLocation(_view.program(), "texture_sampler_3");
	check_gl_error("glGetUniformLocation");
	glUniform1i(texture_3, 3);
	check_gl_error("glUniform1i");

	_fragment_type = glGetUniformLocation(_view.program(), "type");
	check_gl_error("glGetUniformLocation");
	set_outline_content_colour(_view.program());
	set_crop_guess_colour(_view.program());

	auto ublas_to_gl = [](boost::numeric::ublas::matrix<double> const& ublas, GLfloat* gl) {
		gl[0] = static_cast<float>(ublas(0, 0));
		gl[1] = static_cast<float>(ublas(0, 1));
		gl[2] = static_cast<float>(ublas(0, 2));
		gl[3] = 0.0f;
		gl[4] = static_cast<float>(ublas(1, 0));
		gl[5] = static_cast<float>(ublas(1, 1));
		gl[6] = static_cast<float>(ublas(1, 2));
		gl[7] = 0.0f;
		gl[8] = static_cast<float>(ublas(2, 0));
		gl[9] = static_cast<float>(ublas(2, 1));
		gl[10] = static_cast<float>(ublas(2, 2));
		gl[11] = 0.0f;
		gl[12] = 0.0f;
		gl[13] = 0.0f;
		gl[14] = 0.0f;
		gl[15] = 1.0f;
	};

	{
		auto conversion = dcp::ColourConversion::rec709_to_xyz();
		boost::numeric::ublas::matrix<double> matrix = conversion.xyz_to_rgb();
		GLfloat gl_matrix[16];
		ublas_to_gl(matrix, gl_matrix);

		auto xyz_rec709_colour_conversion = glGetUniformLocation(_view.program(), "xyz_rec709_colour_conversion");
		check_gl_error("glGetUniformLocation");
		glUniformMatrix4fv(xyz_rec709_colour_conversion, 1, GL_TRUE, gl_matrix);
	}

	{
		auto xyz_rec709 = dcp::ColourConversion::rec709_to_xyz().xyz_to_rgb();
		auto rec2020_xyz = dcp::ColourConversion::rec2020_to_xyz().rgb_to_xyz();
		auto product = boost::numeric::ublas::prod(xyz_rec709, rec2020_xyz);

		GLfloat gl_matrix[16];
		ublas_to_gl(product, gl_matrix);

		auto rec2020_rec709_colour_conversion = glGetUniformLocation(_view.program(), "rec2020_rec709_colour_conversion");
		check_gl_error("glGetUniformLocation");
		glUniformMatrix4fv(rec2020_rec709_colour_conversion, 1, GL_TRUE, gl_matrix);
	}

	glLineWidth(1.0f);
	check_gl_error("glLineWidth");
	glEnable(GL_BLEND);
	check_gl_error("glEnable");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	check_gl_error("glBlendFunc");

	/* Reserve space for the GL_ARRAY_BUFFER */
	glBufferData(GL_ARRAY_BUFFER, 16 * 5 * sizeof(float), nullptr, GL_STATIC_DRAW);
	check_gl_error("glBufferData");
}


void
GLVideoView::set_outline_content_colour(GLuint program)
{
	auto uniform = glGetUniformLocation(program, "outline_content_colour");
	check_gl_error("glGetUniformLocation");
	auto colour = outline_content_colour();
	glUniform4f(uniform, colour.Red() / 255.0f, colour.Green() / 255.0f, colour.Blue() / 255.0f, 1.0f);
	check_gl_error("glUniform4f");
}


void
GLVideoView::set_crop_guess_colour(GLuint program)
{
	auto uniform = glGetUniformLocation(program, "crop_guess_colour");
	check_gl_error("glGetUniformLocation");
	auto colour = crop_guess_colour();
	glUniform4f(uniform, colour.Red() / 255.0f, colour.Green() / 255.0f, colour.Blue() / 255.0f, 1.0f);
	check_gl_error("glUniform4f");
}


void
GLVideoView::draw()
{
	auto pv = player_video().first;
	if (pv) {
		set_image(pv);
	}

	auto pad = pad_colour();
	glClearColor(pad.Red() / 255.0, pad.Green() / 255.0, pad.Blue() / 255.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	check_gl_error("glClear");

	auto const size = _view.size();
	int const width = size.GetWidth();
	int const height = size.GetHeight();

	if (width < 64 || height < 0) {
		return;
	}

	if (_optimisation == Optimisation::MPEG2) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::YUV420P_IMAGE));
	} else if (_optimisation == Optimisation::JPEG2000) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::XYZ_IMAGE));
	} else if (_rec2020) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::REC2020_IMAGE));
	} else {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::REC709_IMAGE));
	}
	for (auto& texture: _video_textures) {
		texture->bind();
	}
	glDrawElements(GL_TRIANGLES, indices_video_texture_number, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_video_texture_offset * sizeof(int)));
	if (_have_subtitle_to_render) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::REC709_SUBTITLE));
		_subtitle_texture->bind();
		glDrawElements(GL_TRIANGLES, indices_subtitle_texture_number, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_subtitle_texture_offset * sizeof(int)));
	}
	if (_viewer->outline_content()) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::OUTLINE_CONTENT));
		glDrawElements(GL_LINES, indices_outline_content_number, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_outline_content_offset * sizeof(int)));
		check_gl_error("glDrawElements");
	}
	if (auto guess = _viewer->crop_guess()) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::CROP_GUESS));
		glDrawElements(GL_LINES, indices_crop_guess_number, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_crop_guess_offset * sizeof(int)));
		check_gl_error("glDrawElements");
	}

	if (pv) {
		_viewer->image_changed(pv);
	}
}


void
GLVideoView::set_image(shared_ptr<const PlayerVideo> pv)
{
	shared_ptr<const Image> video;

	switch (_optimisation) {
	case Optimisation::JPEG2000:
	case Optimisation::MPEG2:
		video = pv->raw_image();
		break;
	case Optimisation::NONE:
		video = pv->image(boost::bind(&PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, true);
		break;
	}

	/* Only the player's black frames should be aligned at this stage, so this should
	 * almost always have no work to do.
	 */
	video = Image::ensure_alignment(video, Image::Alignment::COMPACT);

	/** If _optimisation is J2K we render a XYZ image, doing the colourspace
	 *  conversion, scaling and video range conversion in the GL shader.
	 *  Similarly for MPEG2 we do YUV -> RGB and scaling in the shader.
	 *  Otherwise we render a RGB image without any shader-side processing.
	 */

	if (_optimisation == Optimisation::MPEG2) {
		for (int i = 0; i < 3; ++i) {
			_video_textures[i]->set(video, i);
		}
	} else {
		_video_textures[0]->set(video, 0);
	}

	auto const text = pv->text();
	_have_subtitle_to_render = static_cast<bool>(text) && _optimisation != Optimisation::NONE;
	if (_have_subtitle_to_render) {
		/* opt: only do this if it's a new subtitle? */
		DCPOMATIC_ASSERT(text->image->alignment() == Image::Alignment::COMPACT);
		_subtitle_texture->set(text->image);
	}

	auto const canvas_size = _view.size();
	int const canvas_width = canvas_size.GetWidth();
	int const canvas_height = canvas_size.GetHeight();
	auto const inter_position = pv->inter_position();
	auto const inter_size = pv->inter_size();
	auto const out_size = pv->out_size();
	auto const crop_guess = _viewer->crop_guess();

	auto x_offset = std::max(0, (canvas_width - out_size.width) / 2);
	auto y_offset = std::max(0, (canvas_height - out_size.height) / 2);

	_last_canvas_size.set_next(canvas_size);
	_last_video_size.set_next(video->size());
	_last_inter_position.set_next(inter_position);
	_last_inter_size.set_next(inter_size);
	_last_out_size.set_next(out_size);
	_last_crop_guess.set_next(crop_guess);

	auto const sizing_changed = _last_canvas_size.changed() || _last_inter_position.changed() || _last_inter_size.changed() || _last_out_size.changed();

	using namespace dcpomatic::gl;

	if (sizing_changed) {
		const auto video = _optimisation == Optimisation::NONE
			? Rectangle(canvas_size, x_offset, y_offset, out_size)
			: Rectangle(canvas_size, inter_position.x + x_offset, inter_position.y + y_offset, inter_size);

		glBufferSubData(GL_ARRAY_BUFFER, array_buffer_video_offset, video.size(), video.vertices());
		check_gl_error("glBufferSubData (video)");

		const auto outline_content = Rectangle(canvas_size, inter_position.x + x_offset, inter_position.y + y_offset, inter_size);
		glBufferSubData(GL_ARRAY_BUFFER, array_buffer_outline_content_offset, outline_content.size(), outline_content.vertices());
		check_gl_error("glBufferSubData (outline_content)");
	}

	if ((sizing_changed || _last_crop_guess.changed()) && crop_guess) {
		auto const crop_guess_rectangle = Rectangle(
			canvas_size,
			inter_position.x + x_offset + inter_size.width * crop_guess->x,
			inter_position.y + y_offset + inter_size.height * crop_guess->y,
			dcp::Size(inter_size.width * crop_guess->width, inter_size.height * crop_guess->height)
			);
		glBufferSubData(GL_ARRAY_BUFFER, array_buffer_crop_guess_offset, crop_guess_rectangle.size(), crop_guess_rectangle.vertices());
		check_gl_error("glBufferSubData (crop_guess_rectangle)");
	}

	if (_have_subtitle_to_render) {
		const auto subtitle = Rectangle(canvas_size, inter_position.x + x_offset + text->position.x, inter_position.y + y_offset + text->position.y, text->image->size());
		glBufferSubData(GL_ARRAY_BUFFER, array_buffer_subtitle_offset, subtitle.size(), subtitle.vertices());
		check_gl_error("glBufferSubData (subtitle)");
	}

	_rec2020 = pv->colour_conversion() && pv->colour_conversion()->about_equal(dcp::ColourConversion::rec2020_to_xyz(), 1e-6);

	/* opt: where should these go? */
	for (auto i = 0; i < 3; ++i) {
		_video_textures[i]->bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		check_gl_error("glTexParameteri");

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		check_gl_error("glTexParameterf");
	}

	_subtitle_texture->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_gl_error("glTexParameteri");

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	check_gl_error("glTexParameterf");
}


void
GLVideoView::start()
{
	VideoView::start();
	_view.start();
	_meters.start();
}

void
GLVideoView::stop()
{
	_view.stop();
	_meters.stop();
}


void
GLVideoView::play_pre_draw()
{
	if (length() != dcpomatic::DCPTime()) {
		auto const next = position() + one_video_frame();

		if (next >= length()) {
			_viewer->finished();
			return;
		}

		get_next_frame(false);
	}
}


int
GLVideoView::play_post_draw()
{
	while (true) {
		auto const n = time_until_next_frame();
		if (!n || *n > 5) {
			break;
		}
		get_next_frame(true);
		add_dropped();
	}

	return time_until_next_frame().get_value_or(0);
}


void
GLVideoView::create_textures()
{
	for (int i = 0; i < 3; ++i) {
		std::unique_ptr<Texture> texture(new Texture(_optimisation == Optimisation::JPEG2000 ? 2 : 1, i));
		_video_textures.push_back(std::move(texture));
	}
	_subtitle_texture.reset(new Texture(1, 3));
}


VideoView::NextFrameResult
GLVideoView::display_next_frame(bool non_blocking)
{
	NextFrameResult const r = get_next_frame(non_blocking);
	_view.request_draw();
	return r;
}


Texture::Texture(GLint unpack_alignment, int unit)
	: _unpack_alignment(unpack_alignment)
	, _unit(unit)
{
	glGenTextures(1, &_name);
	check_gl_error("glGenTextures");
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


void
GLVideoView::update()
{
	_view.request_draw();
}


#endif

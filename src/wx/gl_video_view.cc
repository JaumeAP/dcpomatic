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
using namespace dcpomatic::gl;


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



GLVideoView::GLVideoView(FilmViewer* viewer, wxWindow *parent, bool wake)
	: VideoView(viewer, wake)
	, _view(
		parent,
		fragment_source,
		boost::bind(&GLVideoView::setup_shaders, this),
		boost::bind(&GLVideoView::setup_textures, this),
		boost::bind(&GLVideoView::thread_body, this),
		boost::bind(&GLVideoView::update, this)
	)
	, _rec2020(false)
	, _image(_coords.add_filled_rectangle())
	, _outline_content(_coords.add_outline_rectangle())
	, _crop_guess(_coords.add_outline_rectangle())
	, _subtitle(_coords.add_filled_rectangle())
	, _playing(false)
	, _pending_request(Request::NONE)
	, _audio_meters(viewer, parent)
{
	_view.get()->Bind(wxEVT_TIMER, boost::bind(&GLVideoView::check_for_butler_errors, this));
	_view.Sized.connect(boost::bind(boost::ref(Sized)));

	_timer.reset(new wxTimer(_view.get()));
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

	LOG_DEBUG_PLAYER("Latency {}", _viewer->average_latency());
}


void
GLVideoView::update_crop_guess()
{
	/* Assume at least one update() has happened before now */
	request(Request::SET_CROP_GUESS_AND_DRAW);
	rethrow();
}


/** Called from the UI thread */
void
GLVideoView::update()
{
	request(Request::SET_IMAGE_AND_DRAW);
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

	_coords.setup();

	auto texture_0 = Uniform1i(_view.program(), "texture_sampler_0");
	texture_0.set(0);
	auto texture_1 = Uniform1i(_view.program(), "texture_sampler_1");
	texture_1.set(1);
	auto texture_2 = Uniform1i(_view.program(), "texture_sampler_2");
	texture_2.set(2);
	auto texture_3 = Uniform1i(_view.program(), "texture_sampler_3");
	texture_3.set(3);

	_fragment_type.setup(_view.program(), "type");
	set_outline_content_colour();
	set_crop_guess_colour();

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

		auto xyz_rec709_colour_conversion = UniformMatrix4fv(_view.program(), "xyz_rec709_colour_conversion");
		xyz_rec709_colour_conversion.set(gl_matrix);
	}

	{
		auto xyz_rec709 = dcp::ColourConversion::rec709_to_xyz().xyz_to_rgb();
		auto rec2020_xyz = dcp::ColourConversion::rec2020_to_xyz().rgb_to_xyz();
		auto product = boost::numeric::ublas::prod(xyz_rec709, rec2020_xyz);

		GLfloat gl_matrix[16];
		ublas_to_gl(product, gl_matrix);

		auto rec2020_rec709_colour_conversion = UniformMatrix4fv(_view.program(), "rec2020_rec709_colour_conversion");
		rec2020_rec709_colour_conversion.set(gl_matrix);
	}

	glLineWidth(1.0f);
	check_error("glLineWidth");
	glEnable(GL_BLEND);
	check_error("glEnable");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	check_error("glBlendFunc");
}


void
GLVideoView::set_outline_content_colour()
{
	auto uniform = UniformVec4f(_view.program(), "outline_content_colour");
	auto colour = outline_content_colour();
	uniform.set(colour.Red() / 255.0f, colour.Green() / 255.0f, colour.Blue() / 255.0f, 1.0f);
}


void
GLVideoView::set_crop_guess_colour()
{
	auto uniform = UniformVec4f(_view.program(), "crop_guess_colour");
	auto colour = crop_guess_colour();
	uniform.set(colour.Red() / 255.0f, colour.Green() / 255.0f, colour.Blue() / 255.0f, 1.0f);
}


void
GLVideoView::draw()
{
	auto pad = pad_colour();
	glClearColor(pad.Red() / 255.0, pad.Green() / 255.0, pad.Blue() / 255.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	check_error("glClear");

	auto const size = _view.size();
	int const width = size.GetWidth();
	int const height = size.GetHeight();

	if (width < 64 || height < 0) {
		return;
	}

	glViewport(0, 0, width, height);
	check_error("glViewport");

	glBindVertexArray(_view.vao());
	check_error("glBindVertexArray");
	if (_optimisation == Optimisation::MPEG2) {
		_fragment_type.set(static_cast<GLint>(FragmentType::YUV420P_IMAGE));
	} else if (_optimisation == Optimisation::JPEG2000) {
		_fragment_type.set(static_cast<GLint>(FragmentType::XYZ_IMAGE));
	} else if (_rec2020) {
		_fragment_type.set(static_cast<GLint>(FragmentType::REC2020_IMAGE));
	} else {
		_fragment_type.set(static_cast<GLint>(FragmentType::REC709_IMAGE));
	}
	for (auto& texture: _video_textures) {
		texture.bind();
	}
	_image.draw();

	if (_have_subtitle_to_render) {
		_fragment_type.set(static_cast<GLint>(FragmentType::REC709_SUBTITLE));
		_subtitle_texture->bind();
		_subtitle.draw();
	}
	if (_viewer->outline_content()) {
		_fragment_type.set(static_cast<GLint>(FragmentType::OUTLINE_CONTENT));
		_outline_content.draw();
	}
	if (auto guess = _viewer->crop_guess()) {
		_fragment_type.set(static_cast<GLint>(FragmentType::CROP_GUESS));
		_crop_guess.draw();
	}

	glFlush();
	check_error("glFlush");

	_view.get()->SwapBuffers();
}


/** Take a frame and set up the GL textures ready for it to be drawn.
 *  This also prepares subtitles, the crop guess rectangle, etc.
 */
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
		video = pv->image(force(AV_PIX_FMT_RGB24), VideoRange::FULL, true);
		break;
	}

	/* Only the player's black frames should be aligned at this stage, so this should
	 * almost always have no work to do.
	 */
	video = Image::ensure_alignment(video, Image::Alignment::COMPACT);

	/** If _optimisation is J2K we render a XYZ image, doing the colourspace
	 *  conversion, cropping, scaling and video range conversion in the GL shader.
	 *  Similarly for MPEG2 we do YUV -> RGB and scaling in the shader.
	 *  Otherwise we render a RGB image without any shader-side processing.
	 */

	if (_optimisation == Optimisation::MPEG2) {
		for (int i = 0; i < 3; ++i) {
			_video_textures[i].set(video, i);
		}
	} else {
		_video_textures[0].set(video, 0);
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
	auto const crop = pv->crop();
	auto const crop_guess = _viewer->crop_guess();

	auto x_offset = std::max(0, (canvas_width - out_size.width) / 2);
	auto y_offset = std::max(0, (canvas_height - out_size.height) / 2);

	_last_canvas_size.set_next(canvas_size);
	_last_video_size.set_next(video->size());
	_last_inter_position.set_next(inter_position);
	_last_inter_size.set_next(inter_size);
	_last_out_size.set_next(out_size);
	_last_crop.set_next(crop);
	_last_crop_guess.set_next(crop_guess);

	auto const sizing_changed = _last_canvas_size.changed() ||
		_last_inter_position.changed() ||
		_last_inter_size.changed() ||
		_last_out_size.changed() ||
		_last_crop.changed();

	if (sizing_changed) {
		if (_optimisation == Optimisation::NONE) {
			_image.set(canvas_size, x_offset, y_offset, out_size);
		} else {
			_image.set(canvas_size, inter_position.x + x_offset, inter_position.y + y_offset, inter_size);
		}

		_outline_content.set(canvas_size, inter_position.x + x_offset, inter_position.y + y_offset, inter_size);

	}
	if ((sizing_changed || _last_crop_guess.changed()) && crop_guess) {
		_crop_guess.set(
			canvas_size,
			inter_position.x + x_offset + inter_size.width * crop_guess->x,
			inter_position.y + y_offset + inter_size.height * crop_guess->y,
			dcp::Size(inter_size.width * crop_guess->width, inter_size.height * crop_guess->height),
			crop,
			video->size()
			);
	}

	if (_have_subtitle_to_render) {
		_subtitle.set(
			canvas_size,
			inter_position.x + x_offset + text->position.x,
			inter_position.y + y_offset + text->position.y,
			text->image->size(),
			crop,
			video->size()
		);
	}

	_rec2020 = pv->colour_conversion() && pv->colour_conversion()->about_equal(dcp::ColourConversion::rec2020_to_xyz(), 1e-6);

	/* opt: where should these go? */
	for (auto i = 0; i < 3; ++i) {
		_video_textures[i].bind();
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		check_error("glTexParameteri");

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		check_error("glTexParameterf");
	}

	_subtitle_texture->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_error("glTexParameteri");

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	check_error("glTexParameterf");

	if (sizing_changed || _have_subtitle_to_render) {
		_coords.set_array_buffer();
	}

	_last_canvas_size.update();
	_last_video_size.update();
	_last_inter_position.update();
	_last_inter_size.update();
	_last_out_size.update();
	_last_crop.update();
	_last_crop_guess.update();
}


void
GLVideoView::start()
{
	VideoView::start();

	boost::mutex::scoped_lock lm(_playing_mutex);
	_playing = true;
	_thread_work_condition.notify_all();
}

void
GLVideoView::stop()
{
	boost::mutex::scoped_lock lm(_playing_mutex);
	_playing = false;
}


void
GLVideoView::thread_playing()
{
	if (length() != dcpomatic::DCPTime()) {
		auto const next = position() + one_video_frame();

		if (next >= length()) {
			_viewer->finished();
			return;
		}

		get_next_frame(false);
		set_image_and_draw();
	}

	while (true) {
		optional<int> n = time_until_next_frame();
		if (!n || *n > 5) {
			break;
		}
		get_next_frame(true);
		add_dropped();
	}
}


void
GLVideoView::set_image_and_draw()
{
	auto pv = player_video().first;
	if (pv) {
		set_image(pv);
	}

	draw();

	if (pv) {
		_viewer->image_changed(pv);
	}
}


void
GLVideoView::thread_body()
{
	boost::mutex::scoped_lock lm(_playing_mutex);
	while (!_playing && _pending_request == Request::NONE) {
		_thread_work_condition.wait(lm);
	}
	lm.unlock();

	if (_playing) {
		thread_playing();
	} else if (_pending_request == Request::SET_IMAGE_AND_DRAW) {
		_pending_request = Request::NONE;
		set_image_and_draw();
	} else if (_pending_request == Request::SET_CROP_GUESS_AND_DRAW) {
		_pending_request = Request::NONE;
		if (auto pv = player_video().first) {
			set_image(pv);
		}
		draw();
	}

	boost::this_thread::interruption_point();
	dcpomatic_sleep_milliseconds(time_until_next_frame().get_value_or(0));
}


VideoView::NextFrameResult
GLVideoView::display_next_frame(bool non_blocking)
{
	NextFrameResult const r = get_next_frame(non_blocking);
	request(Request::SET_IMAGE_AND_DRAW);
	return r;
}


void
GLVideoView::request(Request request)
{
	if (_pending_request == Request::SET_IMAGE_AND_DRAW) {
		/* Don't replace this; it's a superset of other requests */
		return;
	}

	boost::mutex::scoped_lock lm(_playing_mutex);
	_pending_request = request;
	_thread_work_condition.notify_all();
}


void
GLVideoView::setup_textures()
{
	for (int i = 0; i < 3; ++i) {
		_video_textures.emplace_back(_optimisation == Optimisation::JPEG2000 ? 2 : 1, i);
	}
	_subtitle_texture.emplace(Texture(1, 3));
}

#endif

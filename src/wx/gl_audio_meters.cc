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


#include "audio_meter_util.h"
#include "film_viewer.h"
#include "gl_audio_meters.h"
#include "gl_util.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/constants.h"
#include "lib/image_png.h"
LIBDCP_DISABLE_WARNINGS
#include <wx/dcmemory.h>
#include <wx/glcanvas.h>
#include <wx/graphics.h>
#include <wx/rawbmp.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS

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


using std::make_shared;
using std::vector;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::gl;


static constexpr char fragment_source[] =
"#version 330 core\n"
"\n"
"in vec2 TexCoord;\n"
"\n"
"uniform sampler2D texture_sampler;\n"
"\n"
/* type = 0: draw meter bar
 * type = 1: draw label texture
 * type = 2: draw scale lines
 * See FragmentType enum below.
 */
"uniform int type = 0;\n"
"uniform vec4 meter_bar_colour;\n"
"uniform vec4 meter_line_colour;\n"
"uniform int height;\n"
"\n"
"out vec4 FragColor;\n"
"\n"
"void main()\n"
"{\n"
"	switch (type) {\n"
"		case 0:\n"
"                       float scale = gl_FragCoord.y / height;\n"
"			FragColor = meter_bar_colour * vec4(scale, scale, scale, 1);\n"
"			break;\n"
"               case 1:\n"
"			FragColor = texture(texture_sampler, TexCoord);\n"
"                       break;\n"
"               case 2:\n"
"			FragColor = meter_line_colour;\n"
"                       break;\n"
"	}\n"
"}\n";


enum class FragmentType
{
	METER_BARS = 0,
	LABEL = 1,
	SCALE_LINE = 2,
};


auto constexpr label_texture_width = 32;
auto constexpr label_texture_height = 16;


GLAudioMeters::GLAudioMeters(FilmViewer* viewer, wxWindow* parent)
	: _view(
		parent,
		fragment_source,
		boost::bind(&GLAudioMeters::setup_shaders, this),
		boost::bind(&GLAudioMeters::setup_textures, this),
		boost::bind(&GLAudioMeters::thread_body, this),
		boost::bind(&GLAudioMeters::update, this)
	)
	, _viewer(viewer)
{

}


void
GLAudioMeters::setup_shaders()
{
	vector<unsigned int> indices;
	for (auto i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		auto rectangle = _coords.add_filled_rectangle();
		_meter_bars.push_back(rectangle);
		_meter_bars_group.add(rectangle);
	}
	for (auto i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		_channel_labels.push_back(_coords.add_filled_rectangle());
	}
	for (auto i = 0U; i < dcpomatic::audio_meter::scales().size(); ++i) {
		_scale_labels.push_back(_coords.add_filled_rectangle());
		_scale_lines.push_back(_coords.add_line());
	}

	_coords.setup();

	_fragment_type.setup(_view.program(), "type");
	_texture_sampler.setup(_view.program(), "texture_sampler");

	_meter_bar_colour.setup(_view.program(), "meter_bar_colour");
	_meter_bar_colour.set(0, 1, 0, 1.0f);

	_meter_line_colour.setup(_view.program(), "meter_line_colour");
	_meter_line_colour.set(0.7, 0.7, 0.7, 1.0f);

	_height.setup(_view.program(), "height");

	glLineWidth(0.5f);
	check_error("glLineWidth");
	glEnable(GL_BLEND);
	check_error("glEnable");
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	check_error("glBlendFunc");
}


void
GLAudioMeters::get_next_frame()
{
	if (auto levels = _viewer->butler()->get_audio_levels()) {
		// std::cout << "got some levels.\n";
		_peaks_time = levels->time;
		_peaks = levels->peaks;
	} else {
		// std::cout << "got no levels.\n";
		_peaks_time = boost::none;
		_peaks.clear();
	}
}


void
GLAudioMeters::update()
{
	/* XXX */
}


void
GLAudioMeters::thread_body()
{
	auto butler = _viewer->butler();
	if (!butler) {
		return;
	}
	std::cout << "thread_body got butler with " << butler.use_count() << "\n";

	auto const gap = butler->time_between_audio_level_measurements();
	auto const half_gap = gap / 2;

	auto const viewer = _viewer->position();

	while (!_peaks_time || (*_peaks_time < viewer && (viewer - *_peaks_time) > half_gap)) {
		/* The peaks we have are a bit behind the viewer, so try and get some more */
		get_next_frame();
	}

	if (_peaks_time) {
		std::cout << "draw: " << to_string(*_peaks_time) << " " << to_string(viewer) << "\n";
	}

	draw();

	if (!_peaks_time || (*_peaks_time - viewer) > gap) {
		boost::this_thread::sleep_for(boost::chrono::milliseconds(static_cast<int>(half_gap.seconds() * 1000)));
	} else {
		boost::this_thread::sleep_for(boost::chrono::milliseconds(static_cast<int>(dcpomatic::DCPTime(*_peaks_time - viewer + gap).seconds() * 1000)));
	}
}


void
GLAudioMeters::draw()
{
	auto constexpr meter_gap = 2;

	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	check_error("glClear");

	if (_peaks.empty()) {
		glFlush();
		check_error("glFlush");
		return;
	}

	using namespace dcpomatic::audio_meter;

	auto const width = _view.size().GetWidth();
	auto const height = _view.size().GetHeight();
	auto const meter_height = height - border() * 2 - channel_label_height();
	auto const channel_width = (width - scale_label_width() - border() * 2) / _peaks.size();

	_height.set(height);

	/* Peak rectangles that we have data for */
	auto const have_data = _peaks.size();
	for (auto i = 0U; i < have_data; ++i) {
		auto const y = db_to_y(meter_height, linear_level_to_db(_peaks[i]));
		_meter_bars[i].set(
			_view.size(),
			border() + scale_label_width() + i * channel_width + meter_gap / 2,
			y + border(),
			wxSize{static_cast<int>(channel_width) - meter_gap, meter_height - y}
		);
	}

	/* Scale coordinate labels and lines */
	int index = 0;
	for (auto const& i: scales()) {
		auto const y = db_to_y(meter_height, i.first);
		_scale_labels[index].set(
			_view.size(),
			border() + scale_label_width() - _scale_label_widths[index] - 4,
			y + border(),
			wxSize{label_texture_width, label_texture_height}
		);
		_scale_lines[index].set(_view.size(), 0, y + border(), width, y + border());
		++index;
	}

	/* Channel labels */
	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		_channel_labels[i].set(
			_view.size(),
			border() + scale_label_width() + channel_width * i + (channel_width - _channel_label_widths[i]) / 2,
			_view.size().GetHeight() - channel_label_height(),
			wxSize{label_texture_width, label_texture_height}
		);
	}

	_coords.set_array_buffer();

	/* Draw the scales first so that the meter bars can overlay them */
	_fragment_type.set(static_cast<GLint>(FragmentType::LABEL));
	for (auto i = 0U; i < dcpomatic::audio_meter::scales().size(); ++i) {
		_scale_label_textures[i].bind();
		_texture_sampler.set(i);
		_scale_labels[i].draw();
	}

	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		_channel_label_textures[i].bind();
		_texture_sampler.set(dcpomatic::audio_meter::scales().size() + i);
		_channel_labels[i].draw();
	}

	_fragment_type.set(static_cast<GLint>(FragmentType::SCALE_LINE));
	for (auto i = 0U; i < dcpomatic::audio_meter::scales().size(); ++i) {
		_scale_lines[i].draw();
	}

	_fragment_type.set(static_cast<GLint>(FragmentType::METER_BARS));
	_meter_bars_group.draw();

	glFlush();
	check_error("glFlush");

	_view.get()->SwapBuffers();
}


wxWindow*
GLAudioMeters::get() const
{
	return _view.get();
}


void
GLAudioMeters::setup_textures()
{
	auto label = [](wxString text) {

		wxDouble width;

		wxBitmap bitmap(label_texture_width, label_texture_height, 24);
		{
			wxMemoryDC dc(bitmap);
			auto gc = wxGraphicsContext::Create(dc);
			DCPOMATIC_ASSERT(gc);

			gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
			gc->SetFont(gc->CreateFont(*wxSMALL_FONT, *wxWHITE));

			dc.SetPen(*wxGREY_PEN);
			gc->DrawText(text, 0, 0);

			wxDouble height;
			wxDouble descent;
			wxDouble leading;
			gc->GetTextExtent(text, &width, &height, &descent, &leading);

			delete gc;
		}

		auto image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size{label_texture_width, label_texture_height}, Image::Alignment::COMPACT);
		auto* p = image->data()[0];

		wxNativePixelData data(bitmap);
		wxNativePixelData::Iterator row(data);

		for (auto y = 0; y < label_texture_height; ++y) {
			auto q = row;
			for (auto x = 0; x < label_texture_width; ++x) {
				*p++ = q.Red();
				*p++ = q.Green();
				*p++ = q.Blue();
				++q;
			}

			row.OffsetY(data, 1);
		}

		return make_pair(image, width);
	};


	int unit = 0;

	for (auto scale: dcpomatic::audio_meter::scales()) {
		auto image_and_width = label(scale.second);

		_scale_label_textures.emplace_back(1, unit++);
		_scale_label_textures.back().set(image_and_width.first);
		_scale_label_widths.push_back(image_and_width.second);
		_scale_label_textures.back().bind();

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		check_error("glTexParameteri");

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		check_error("glTexParameterf");
	}

	for (int i = 0; i < MAX_DCP_AUDIO_CHANNELS; ++i) {
		auto image_and_width = label(std_to_wx(short_audio_channel_name(i)));

		_channel_label_textures.emplace_back(1, unit++);
		_channel_label_textures.back().set(image_and_width.first);
		_channel_label_widths.push_back(image_and_width.second);
		_channel_label_textures.back().bind();

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		check_error("glTexParameteri");

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		check_error("glTexParameterf");
	}
}


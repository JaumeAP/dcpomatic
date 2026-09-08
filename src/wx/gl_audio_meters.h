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
#include "gl_view.h"
#include "lib/dcpomatic_time.h"
#include <dcp/warnings.h>


class FilmViewer;


/* See note in gl_view.h */
#if wxCHECK_VERSION(3,1,0)


class GLAudioMeters
{
public:
	GLAudioMeters(FilmViewer* viewer, wxWindow* parent);

	GLAudioMeters(GLAudioMeters const&) = delete;
	GLAudioMeters& operator=(GLAudioMeters const&) = delete;

	wxWindow* get() const;

private:
	void thread_body();
	void draw();
	void get_next_frame();
	void update();

	void setup_shaders();
	void setup_textures();

	GLView _view;
	FilmViewer* _viewer;

	boost::optional<dcpomatic::DCPTime> _peaks_time;
	std::vector<float> _peaks;

	dcpomatic::gl::Uniform1i _fragment_type;
	dcpomatic::gl::Uniform1i _texture_sampler;
	dcpomatic::gl::UniformVec4f _meter_bar_colour;
	dcpomatic::gl::UniformVec4f _meter_line_colour;
	dcpomatic::gl::Uniform1i _height;

	/** Number of rectangles we need nodes and coordinates for */
	int _rectangles = 0;
	/** Number of lines we need nodes and coordinates for */
	int _lines = 0;

	std::vector<dcpomatic::gl::Texture> _scale_label_textures;
	std::vector<int> _scale_label_widths;
	std::vector<dcpomatic::gl::Texture> _channel_label_textures;
	std::vector<int> _channel_label_widths;

	dcpomatic::gl::Coords _coords;
	std::vector<dcpomatic::gl::Coords::FilledRectangle> _meter_bars;
	dcpomatic::gl::Coords::FilledRectangleGroup _meter_bars_group;
	std::vector<dcpomatic::gl::Coords::FilledRectangle> _channel_labels;
	std::vector<dcpomatic::gl::Coords::FilledRectangle> _scale_labels;
	std::vector<dcpomatic::gl::Coords::Line> _scale_lines;
};


#endif

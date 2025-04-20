/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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


#include "gl_view.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/glcanvas.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS

#undef None
#undef Success
#undef Status


/* This will only build on an new-enough wxWidgets: see the comment in gl_view.h */
#if wxCHECK_VERSION(3,1,0)


#include "video_view.h"
#include "lib/signaller.h"
#include "lib/position.h"
#include <dcp/util.h>
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>


class Texture
{
public:
	Texture(GLint unpack_alignment, int unit = 0);
	~Texture();

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


class GLVideoView : public VideoView
{
public:
	GLVideoView(FilmViewer* viewer, wxWindow* parent);

	wxWindow* get() const override {
		return _view.canvas();
	}
	void update() override;
	void start() override;
	void stop() override;

	NextFrameResult display_next_frame(bool) override;

	bool vsync_enabled() const {
		return _view.vsync_enabled();
	}

	std::map<GLenum, std::string> information() const {
		return _information;
	}

private:
	void set_image(std::shared_ptr<const PlayerVideo> pv);
	void check_for_butler_errors();
	void ensure_context();
	void set_outline_content_colour(GLuint program);
	void set_crop_guess_colour(GLuint program);

	void setup_shaders();
	void create_textures();
	void play_pre_draw();
	void draw();
	int play_post_draw();

	GLView _view;

	template <class T>
	class Last
	{
	public:
		void set_next(T const& next) {
			_next = next;
		}

		bool changed() const {
			return !_value || *_value != _next;
		}

		void update() {
			_value = _next;
		}

	private:
		boost::optional<T> _value;
		T _next;
	};

	Last<wxSize> _last_canvas_size;
	Last<dcp::Size> _last_video_size;
	Last<Position<int>> _last_inter_position;
	Last<dcp::Size> _last_inter_size;
	Last<dcp::Size> _last_out_size;
	Last<boost::optional<dcpomatic::Rect<float>>> _last_crop_guess;

	boost::atomic<bool> _rec2020;
	std::vector<std::unique_ptr<Texture>> _video_textures;
	std::unique_ptr<Texture> _subtitle_texture;
	bool _have_subtitle_to_render = false;

	GLint _fragment_type;

	std::shared_ptr<wxTimer> _timer;

	std::map<GLenum, std::string> _information;
};

#endif

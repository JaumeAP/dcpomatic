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


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/glcanvas.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS

#undef None
#undef Success
#undef Status


/* The OpenGL API in wxWidgets 3.0.x is sufficiently different to make it awkward to support,
 * and I think it may even have things missing that we require (e.g. the attributes parameter
 * to wxGLContext).  Hence we only support the GLVideoView on wxWidgets 3.1.0 and higher
 * (which only excludes the old macOS builds, since wxWidgets 3.1.x does not support macOS
 * 10.9 or earlier).
 */
#if wxCHECK_VERSION(3,1,0)


#include "gl_audio_meters.h"
#include "gl_util.h"
#include "gl_view.h"
#include "video_view.h"
#include "lib/crop.h"
#include "lib/position.h"
#include <dcp/util.h>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <atomic>


class GLVideoView : public VideoView
{
public:
	GLVideoView(FilmViewer* viewer, wxWindow* parent, bool wake);

	wxWindow* get() const override {
		return _view.get();
	}

	wxWindow* get_audio_meters() const override {
		return _audio_meters.get();
	}

	void update() override;
	void update_crop_guess() override;
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
	enum class Request {
		NONE,
		SET_IMAGE_AND_DRAW,
		SET_CROP_GUESS_AND_DRAW,
	};

	void set_image(std::shared_ptr<const PlayerVideo> pv);
	void set_image_and_draw();
	void draw();
	void thread_body();
	void thread_playing();
	void request(Request request);
	void check_for_butler_errors();
	void setup_shaders();
	void set_outline_content_colour();
	void set_crop_guess_colour();
	void setup_textures();

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
	Last<Crop> _last_crop;
	Last<boost::optional<dcpomatic::Rect<float>>> _last_crop_guess;

	std::atomic<bool> _rec2020;
	std::vector<dcpomatic::gl::Texture> _video_textures;
	boost::optional<dcpomatic::gl::Texture> _subtitle_texture;
	bool _have_subtitle_to_render = false;

	dcpomatic::gl::Coords _coords;
	dcpomatic::gl::Coords::FilledRectangle _image;
	dcpomatic::gl::Coords::OutlineRectangle _outline_content;
	dcpomatic::gl::Coords::OutlineRectangle _crop_guess;
	dcpomatic::gl::Coords::FilledRectangle _subtitle;

	boost::mutex _playing_mutex;
	boost::condition _thread_work_condition;
	std::atomic<bool> _playing;
	std::atomic<Request> _pending_request;

	dcpomatic::gl::Uniform1i _fragment_type;

	std::shared_ptr<wxTimer> _timer;

	std::map<GLenum, std::string> _information;

	GLAudioMeters _audio_meters;
};

#endif

/*
    Copyright (C) 2013-2019 Carl Hetherington <cth@carlh.net>

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

#include "player_video.h"
#include "content.h"
#include "video_content.h"
#include "image.h"
#include "image_proxy.h"
#include "j2k_image_proxy.h"
#include "film.h"
#include "render_text.h"
#include "text_content.h"
#include <dcp/raw_convert.h>
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <libxml++/libxml++.h>
#include <iostream>

using std::string;
using std::cout;
using std::pair;
using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
using boost::function;
using dcp::Data;
using dcp::raw_convert;

PlayerVideo::PlayerVideo (
	shared_ptr<const ImageProxy> in,
	Crop crop,
	boost::optional<double> fade,
	dcp::Size inter_size,
	dcp::Size out_size,
	Eyes eyes,
	Part part,
	optional<ColourConversion> colour_conversion,
	VideoRange video_range,
	weak_ptr<Content> content,
	optional<dcpomatic::DCPTime> time,
	int video_frame_rate
	)
	: _in (in)
	, _crop (crop)
	, _fade (fade)
	, _inter_size (inter_size)
	, _out_size (out_size)
	, _eyes (eyes)
	, _part (part)
	, _colour_conversion (colour_conversion)
	, _video_range (video_range)
	, _content (content)
	, _time (time)
	, _video_frame_rate (video_frame_rate)
	, _image_dirty (false)
{

}

PlayerVideo::PlayerVideo (shared_ptr<cxml::Node> node, shared_ptr<Socket> socket)
	: _image_dirty (false)
{
	_crop = Crop (node);
	_fade = node->optional_number_child<double> ("Fade");

	_inter_size = dcp::Size (node->number_child<int> ("InterWidth"), node->number_child<int> ("InterHeight"));
	_out_size = dcp::Size (node->number_child<int> ("OutWidth"), node->number_child<int> ("OutHeight"));
	_eyes = (Eyes) node->number_child<int> ("Eyes");
	_part = (Part) node->number_child<int> ("Part");
	_video_range = (VideoRange) node->number_child<int>("VideoRange");

	/* Assume that the ColourConversion uses the current state version */
	_colour_conversion = ColourConversion::from_xml (node, Film::current_state_version);

	_in = image_proxy_factory (node->node_child ("In"), socket);

	if (node->optional_number_child<int> ("SubtitleX")) {

		shared_ptr<Image> image (
			new Image (AV_PIX_FMT_BGRA, dcp::Size (node->number_child<int> ("SubtitleWidth"), node->number_child<int> ("SubtitleHeight")), true)
			);

		image->read_from_socket (socket);

		/* XXX_b
		_text = PositionImage (image, Position<int> (node->number_child<int> ("SubtitleX"), node->number_child<int> ("SubtitleY")));
		*/
	}
}

void
PlayerVideo::set_text (list<PlayerText> text)
{
	_text = text;
}

shared_ptr<Image>
PlayerVideo::image (function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast) const
{
	/* XXX: this assumes that image() and prepare() are only ever called with the same parameters (except crop, inter size, out size, fade) */

	boost::mutex::scoped_lock lm (_mutex);
	if (!_image || _image_dirty) {
		make_image (pixel_format, aligned, fast);
	}
	return _image;
}

/** Create an image for this frame.  A lock must be held on _mutex.
 *  @param pixel_format Function which is called to decide what pixel format the output image should be;
 *  it is passed the pixel format of the input image from the ImageProxy, and should return the desired
 *  output pixel format.  Two functions force and keep_xyz_or_rgb are provided for use here.
 *  @param aligned true if the output image should be aligned to 32-byte boundaries.
 *  @param fast true to be fast at the expense of quality.
 */
void
PlayerVideo::make_image (function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast) const
{
	_image_crop = _crop;
	_image_inter_size = _inter_size;
	_image_out_size = _out_size;
	_image_fade = _fade;

	pair<shared_ptr<Image>, int> prox = _in->image (_inter_size);
	shared_ptr<Image> im = prox.first;
	int const reduce = prox.second;

	Crop total_crop = _crop;
	switch (_part) {
	case PART_LEFT_HALF:
		total_crop.right += im->size().width / 2;
		break;
	case PART_RIGHT_HALF:
		total_crop.left += im->size().width / 2;
		break;
	case PART_TOP_HALF:
		total_crop.bottom += im->size().height / 2;
		break;
	case PART_BOTTOM_HALF:
		total_crop.top += im->size().height / 2;
		break;
	default:
		break;
	}

	if (reduce > 0) {
		/* Scale the crop down to account for the scaling that has already happened in ImageProxy::image */
		int const r = pow(2, reduce);
		total_crop.left /= r;
		total_crop.right /= r;
		total_crop.top /= r;
		total_crop.bottom /= r;
	}

	dcp::YUVToRGB yuv_to_rgb = dcp::YUV_TO_RGB_REC601;
	if (_colour_conversion) {
		yuv_to_rgb = _colour_conversion.get().yuv_to_rgb();
	}

	_image = im->crop_scale_window (
		total_crop, _inter_size, _out_size, yuv_to_rgb, _video_range, pixel_format (im->pixel_format()), aligned, fast
		);

	list<PositionImage> subtitles;

	BOOST_FOREACH (PlayerText i, _text) {

		/* Bitmap subtitles */
		BOOST_FOREACH (BitmapText j, i.bitmap) {
			if (!j.image) {
				continue;
			}

			/* j.image will already have been scaled to fit _out_size */
			dcp::Size scaled_size (j.rectangle.width * _out_size.width, j.rectangle.height * _out_size.height);

			subtitles.push_back (
				PositionImage (
					j.image,
					Position<int> (
						lrint (_out_size.width * j.rectangle.x),
						lrint (_out_size.height * j.rectangle.y)
						)
					)
				);
		}

		/* String subtitles (rendered to an image) */
		if (!i.string.empty ()) {
			DCPOMATIC_ASSERT (_time);
			shared_ptr<const TextContent> content = i.content.lock ();
			DCPOMATIC_ASSERT (content);
			list<PositionImage> s = render_text (
				i.string, i.fonts, _out_size, *_time, _video_frame_rate, content->x_offset(), content->y_offset()
				);
			copy (s.begin(), s.end(), back_inserter (subtitles));
		}
	}

	if (!subtitles.empty()) {
		PositionImage pi = merge (subtitles);
		_image->alpha_blend (Image::ensure_aligned(pi.image), pi.position);
	}

	if (_fade) {
		_image->fade (_fade.get ());
	}

	_image_dirty = false;
}

void
PlayerVideo::add_metadata (xmlpp::Node* node) const
{
	_crop.as_xml (node);
	if (_fade) {
		node->add_child("Fade")->add_child_text (raw_convert<string> (_fade.get ()));
	}
	_in->add_metadata (node->add_child ("In"));
	node->add_child("InterWidth")->add_child_text (raw_convert<string> (_inter_size.width));
	node->add_child("InterHeight")->add_child_text (raw_convert<string> (_inter_size.height));
	node->add_child("OutWidth")->add_child_text (raw_convert<string> (_out_size.width));
	node->add_child("OutHeight")->add_child_text (raw_convert<string> (_out_size.height));
	node->add_child("Eyes")->add_child_text (raw_convert<string> (static_cast<int> (_eyes)));
	node->add_child("Part")->add_child_text (raw_convert<string> (static_cast<int> (_part)));
	node->add_child("VideoRange")->add_child_text(raw_convert<string>(static_cast<int>(_video_range)));
	if (_colour_conversion) {
		_colour_conversion.get().as_xml (node);
	}
	/* XXX_b
	if (_text) {
		node->add_child ("SubtitleWidth")->add_child_text (raw_convert<string> (_text->image->size().width));
		node->add_child ("SubtitleHeight")->add_child_text (raw_convert<string> (_text->image->size().height));
		node->add_child ("SubtitleX")->add_child_text (raw_convert<string> (_text->position.x));
		node->add_child ("SubtitleY")->add_child_text (raw_convert<string> (_text->position.y));
	}
	*/
}

void
PlayerVideo::send_binary (shared_ptr<Socket> socket) const
{
	_in->send_binary (socket);
	/* XXX_b
	if (_text) {
		_text->image->write_to_socket (socket);
	}
	*/
}

bool
PlayerVideo::has_j2k () const
{
	/* XXX: maybe other things */

	shared_ptr<const J2KImageProxy> j2k = dynamic_pointer_cast<const J2KImageProxy> (_in);
	if (!j2k) {
		return false;
	}

	return _crop == Crop() && _out_size == j2k->size() && _text.empty() && !_fade && !_colour_conversion;
}

Data
PlayerVideo::j2k () const
{
	shared_ptr<const J2KImageProxy> j2k = dynamic_pointer_cast<const J2KImageProxy> (_in);
	DCPOMATIC_ASSERT (j2k);
	return j2k->j2k ();
}

Position<int>
PlayerVideo::inter_position () const
{
	return Position<int> ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.height) / 2);
}

/** @return true if this PlayerVideo is definitely the same as another, false if it is probably not */
bool
PlayerVideo::same (shared_ptr<const PlayerVideo> other) const
{
	if (_crop != other->_crop ||
	    _fade != other->_fade ||
	    _inter_size != other->_inter_size ||
	    _out_size != other->_out_size ||
	    _eyes != other->_eyes ||
	    _part != other->_part ||
	    _colour_conversion != other->_colour_conversion ||
	    _video_range != other->_video_range) {
		return false;
	}

	if (_text.size() != other->_text.size()) {
		/* Different text counts */
		return false;
	}

	list<PlayerText>::const_iterator i = _text.begin();
	list<PlayerText>::const_iterator j = other->_text.begin();
	while (i != _text.end()) {
		if (*i != *j) {
			/* Same number of texts but one differs */
			return false;
		}
		++i;
		++j;
	}

	return _in->same (other->_in);
}

AVPixelFormat
PlayerVideo::force (AVPixelFormat, AVPixelFormat force_to)
{
	return force_to;
}

AVPixelFormat
PlayerVideo::keep_xyz_or_rgb (AVPixelFormat p)
{
	return p == AV_PIX_FMT_XYZ12LE ? AV_PIX_FMT_XYZ12LE : AV_PIX_FMT_RGB48LE;
}

void
PlayerVideo::prepare (function<AVPixelFormat (AVPixelFormat)> pixel_format, bool aligned, bool fast)
{
	_in->prepare (_inter_size);
	boost::mutex::scoped_lock lm (_mutex);
	if (!_image) {
		make_image (pixel_format, aligned, fast);
	}
}

size_t
PlayerVideo::memory_used () const
{
	return _in->memory_used();
}

/** @return Shallow copy of this; _in and _text are shared between the original and the copy */
shared_ptr<PlayerVideo>
PlayerVideo::shallow_copy () const
{
	return shared_ptr<PlayerVideo>(
		new PlayerVideo(
			_in,
			_crop,
			_fade,
			_inter_size,
			_out_size,
			_eyes,
			_part,
			_colour_conversion,
			_video_range,
			_content,
			_time,
			_video_frame_rate
			)
		);
}

/** Re-read crop, fade, inter/out size, colour conversion and video range from our content.
 *  @return true if this was possible, false if not.
 */
bool
PlayerVideo::reset_metadata (shared_ptr<const Film> film, dcp::Size video_container_size, dcp::Size film_frame_size)
{
	shared_ptr<Content> content = _content.lock();
	if (!content || !_time) {
		return false;
	}

	_crop = content->video->crop();
	_fade = content->video->fade(film, _time->frames_round(_video_frame_rate));
	_inter_size = content->video->scale().size(content->video, video_container_size, film_frame_size);
	_out_size = video_container_size;
	_colour_conversion = content->video->colour_conversion();
	_video_range = content->video->range();

	/* XXX_b _text looks into content directly itself, as maybe the other parameters should; we could just
	   have a pointer to the content and get stuff from there.
	*/

	/* XXX_b thread safety of _image_dirty? */
	_image_dirty = true;

	return true;
}

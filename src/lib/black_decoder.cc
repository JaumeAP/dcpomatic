#include "black_content.h"
#include "black_decoder.h"
#include "image.h"
#include "raw_image_proxy.h"
#include "video_decoder.h"


using std::make_shared;
using std::shared_ptr;
using namespace dcpomatic;


BlackDecoder::BlackDecoder (shared_ptr<const Film> film, shared_ptr<const BlackContent> content)
	: Decoder(film)
	, _content(content)
	, _video_frame_rate(film->video_frame_rate())
{
	video = make_shared<VideoDecoder>(this, content);
	_image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size(128, 128), Image::Alignment::PADDED);
	_image->make_black ();
	_proxy = make_shared<const RawImageProxy>(_image);
}


void
BlackDecoder::seek (ContentTime time, bool)
{
	_position = DCPTime(time.get());
}


bool
BlackDecoder::pass ()
{
	if (_position >= _content->end(film())) {
		return true;
	}

	video->emit(film(), _proxy, _position.frames_round(_video_frame_rate));
	_position += DCPTime::from_frames(1, _video_frame_rate);
	return false;
}


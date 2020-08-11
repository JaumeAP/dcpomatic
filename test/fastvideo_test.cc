#include "lib/colour_conversion.h"
#include "lib/content.h"
#include "lib/cross.h"
#include "lib/image.h"
#include "lib/fastvideo_player_video_preparer.h"
#include "lib/j2k_image_proxy.h"
#include "lib/player_video.h"
#include "lib/types.h"
#include "test.h"
#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/test/unit_test.hpp>
#include <vector>


using std::vector;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;


static
AVPixelFormat
pixel_format (AVPixelFormat)
{
	return AV_PIX_FMT_RGB24;
}


BOOST_AUTO_TEST_CASE (fastvideo_preparer_setup_teardown_test)
{
	FastvideoPlayerVideoPreparer preparer (&pixel_format, true, true);
}


BOOST_AUTO_TEST_CASE (fastvideo_preparer_simple_decode_test)
{
	FastvideoPlayerVideoPreparer preparer (&pixel_format, true, true);

	shared_ptr<J2KImageProxy> proxy(new J2KImageProxy("test/data/sizing_card_flat.j2k", dcp::Size(1998, 1080), AV_PIX_FMT_XYZ12));

	vector<shared_ptr<PlayerVideo> > videos;

	for (int i = 0; i < 16; ++i) {
		shared_ptr<PlayerVideo> pv(
			new PlayerVideo(
				proxy, Crop(), optional<double>(), dcp::Size(1998, 1080), dcp::Size(1998, 1080),
				EYES_BOTH, PART_WHOLE, optional<ColourConversion>(), VIDEO_RANGE_FULL, weak_ptr<Content>(), optional<Frame>(), false
				)
			);

		preparer.request (pv);
		videos.push_back (pv);
	}

	dcpomatic_sleep_seconds (5);

	std::cout << "____-> get an image.\n";
	videos[0]->image_proxy()->image().image->convert_pixel_format(dcp::YUV_TO_RGB_REC709, AV_PIX_FMT_RGBA, true, true)->as_png().write("/home/carl/foo.png");
}


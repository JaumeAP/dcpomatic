#include "lib/poznan_encoder.h"
#include "lib/jpeg2000_encoder.h"
#include "lib/image.h"
#include "lib/colour_conversion.h"
#include "lib/config.h"
#include <dcp/rgb_xyz.h>

using std::vector;
using boost::shared_ptr;

int main ()
{
	JPEG2000Encoder::setup_encoders ();

	shared_ptr<JPEG2000Encoder> encoder = JPEG2000Encoder::from_id ("poznan");

	shared_ptr<Image> rgb (new Image (PIX_FMT_RGB24, dcp::Size (1998, 1080), false));

	int const line_size = 1998 * 3;

	for (int x = 0; x < 666; ++x) {
		for (int y = 0; y < 1080; ++y) {
			rgb->data()[0][y * line_size + x * 3] = 255;
			rgb->data()[0][y * line_size + x * 3 + 1] = 0;
			rgb->data()[0][y * line_size + x * 3 + 2] = 0;
		}
	}

	for (int x = 666; x < 1332; ++x) {
		for (int y = 0; y < 1080; ++y) {
			rgb->data()[0][y * line_size + x * 3] = 0;
			rgb->data()[0][y * line_size + x * 3 + 1] = 255;
			rgb->data()[0][y * line_size + x * 3 + 2] = 0;
		}
	}

	for (int x = 1332; x < 1998; ++x) {
		for (int y = 0; y < 1080; ++y) {
			rgb->data()[0][y * line_size + x * 3] = 0;
			rgb->data()[0][y * line_size + x * 3 + 1] = 0;
			rgb->data()[0][y * line_size + x * 3 + 2] = 255;
		}
	}

	shared_ptr<const dcp::OpenJPEGImage> xyz = dcp::rgb_to_xyz (rgb->data()[0], rgb->size(), rgb->stride()[0], ColourConversion::rec709_to_xyz ());
	Data j2k = encoder->encode (xyz, 100000000, 24, RESOLUTION_2K, false);
	j2k.write ("poznan.j2k");
}

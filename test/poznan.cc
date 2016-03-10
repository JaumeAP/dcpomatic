#include "lib/poznan_encoder.h"
#include "lib/jpeg2000_encoder.h"
#include "lib/image.h"
#include "lib/colour_conversion.h"
#include "lib/config.h"
#include "lib/magick_image_proxy.h"
#include <dcp/rgb_xyz.h>

using std::vector;
using std::cout;
using boost::shared_ptr;
using dcp::Data;

static
void
colour_bars (shared_ptr<Image> rgb)
{
	int const line_size = rgb->stride()[0] / 2;
	int const width = rgb->size().width;
	int const height = rgb->size().height;

	uint16_t* p = reinterpret_cast<uint16_t*> (rgb->data()[0]);

	for (int x = 0; x < width / 3; ++x) {
		for (int y = 0; y < height; ++y) {
			p[y * line_size + x * 3] = 65535;
			p[y * line_size + x * 3 + 1] = 0;
			p[y * line_size + x * 3 + 2] = 0;
		}
	}

	for (int x = width / 3; x < (2 * width / 3); ++x) {
	 	for (int y = 0; y < height; ++y) {
	 		p[y * line_size + x * 3] = 0;
			p[y * line_size + x * 3 + 1] = 65535;
	 		p[y * line_size + x * 3 + 2] = 0;
		}
	}

	for (int x = (2 * width / 3); x < width; ++x) {
	 	for (int y = 0; y < height; ++y) {
			p[y * line_size + x * 3] = 0;
	 		p[y * line_size + x * 3 + 1] = 0;
	 		p[y * line_size + x * 3 + 2] = 65535;
	 	}
	}
}

int main ()
{
	JPEG2000Encoder::setup_encoders ();

	shared_ptr<JPEG2000Encoder> openjpeg = JPEG2000Encoder::from_id ("openjpeg");
	shared_ptr<JPEG2000Encoder> poznan = JPEG2000Encoder::from_id ("poznan");

	bool const testcard = false;

	shared_ptr<Image> rgb;
	if (testcard) {
		rgb.reset (new Image (AV_PIX_FMT_RGB48LE, dcp::Size (1998, 1080), false));
		colour_bars (rgb);
	} else {
		shared_ptr<MagickImageProxy> mip (new MagickImageProxy ("../dcpomatic-test-private/frame.tiff"));
		shared_ptr<Image> raw = mip->image();
		rgb.reset (new Image (AV_PIX_FMT_RGB48LE, raw->size(), false));
		for (int y = 0; y < raw->size().height; ++y) {
			uint8_t const * p = raw->data()[0] + y * raw->stride()[0];
			uint8_t* q = rgb->data()[0] + y * rgb->stride()[0];
			if (y == 0) {
				for (int x = 0; x < 16; ++x) {
					cout << ((int)p[x * 3]) << " " << ((int)p[x * 3 + 1]) << " " << ((int)p[x * 2]) << "\n";
				}
			}
			for (int x = 0; x < raw->size().width; ++x) {
				*q++ = 0;
				*q++ = *p++;
				*q++ = 0;
				*q++ = *p++;
				*q++ = 0;
				*q++ = *p++;
			}
		}
	}

	shared_ptr<const dcp::OpenJPEGImage> xyz = dcp::rgb_to_xyz (rgb->data()[0], rgb->size(), rgb->stride()[0], ColourConversion::rec709_to_xyz ());
	openjpeg->encode (xyz, 100000000, 24, RESOLUTION_2K, false).write ("openjpeg.j2k");
	xyz = dcp::rgb_to_xyz (rgb->data()[0], rgb->size(), rgb->stride()[0], ColourConversion::rec709_to_xyz ());
	poznan->encode (xyz, 100000000, 24, RESOLUTION_2K, false).write ("poznan.j2k");
}

#include "lib/poznan_encoder.h"
#include "lib/jpeg2000_encoder.h"
#include "lib/image.h"
#include "lib/colour_conversion.h"
#include "lib/config.h"
#include <dcp/rgb_xyz.h>

using std::vector;
using std::cout;
using boost::shared_ptr;
using dcp::Data;

int main ()
{
	JPEG2000Encoder::setup_encoders ();

	shared_ptr<JPEG2000Encoder> openjpeg = JPEG2000Encoder::from_id ("openjpeg");
	shared_ptr<JPEG2000Encoder> poznan = JPEG2000Encoder::from_id ("poznan");

	shared_ptr<Image> rgb (new Image (AV_PIX_FMT_RGB48LE, dcp::Size (1998, 1080), false));

	cout << rgb->stride()[0] << "\n";

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

	shared_ptr<const dcp::OpenJPEGImage> xyz = dcp::rgb_to_xyz (rgb->data()[0], rgb->size(), rgb->stride()[0], ColourConversion::rec709_to_xyz ());
	openjpeg->encode (xyz, 100000000, 24, RESOLUTION_2K, false).write ("openjpeg.j2k");
	xyz = dcp::rgb_to_xyz (rgb->data()[0], rgb->size(), rgb->stride()[0], ColourConversion::rec709_to_xyz ());
	poznan->encode (xyz, 100000000, 24, RESOLUTION_2K, false).write ("poznan.j2k");
}

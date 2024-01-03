#include <dcp/openjpeg_image.h>
#include <dcp/j2k_transcode.h>

int main()
{
	auto image = std::make_shared<dcp::OpenJPEGImage>(dcp::Size(1998, 1080));

	for (int y = 0; y < 1080; ++y) {
		for (int x = 0; x < 1998; ++x) {
			for (int c = 0; c < 3; ++c ) {
				image->data(c)[x + y * 1998] = rand() % 4096;
			}
		}
	}

	dcp::compress_j2k(image, 250000000, 24, false, false).write("noise.j2c");
	return 0;
}

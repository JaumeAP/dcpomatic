#include "lib/fastvideo.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE (fastvideo_decoder_test)
{
	dcp::Data data ("test/data/sizing_card_flat.j2k");
	fastvideo_decompress_j2k (data, 0);
}


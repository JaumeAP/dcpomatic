/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "colour_conversion.h"
#include "config.h"
#include "cpu_j2k_frame_encoder.h"
#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_assert.h"
#include "dcpomatic_log.h"
#include "rng.h"
#include <dcp/openjpeg_image.h>
#include <dcp/j2k_transcode.h>

#include "i18n.h"


using std::shared_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::ArrayData;


optional<ArrayData>
CPUJ2KFrameEncoder::encode (DCPVideo const& vf)
{
	optional<ArrayData> encoded;

	try {
		LOG_TIMING("start-local-encode thread=%1 frame=%2", thread_id(), vf.index());
		auto const comment = Config::instance()->dcp_j2k_comment();

		/* This was empirically derived by a user: see #1902 */
		int const minimum_size = 16384;
		LOG_GENERAL ("Using minimum frame size %1", minimum_size);

		auto xyz = convert_to_xyz (vf.frame(), boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2));
		int noise_amount = 2;
		int pixel_skip = 16;
		while (true) {
			encoded = dcp::compress_j2k (
				xyz,
				vf.j2k_bandwidth(),
				vf.frames_per_second(),
				vf.eyes() == Eyes::LEFT || vf.eyes() == Eyes::RIGHT,
				vf.resolution() == Resolution::FOUR_K,
				comment.empty() ? "libdcp" : comment
				);

			if (encoded->size() >= minimum_size) {
				LOG_GENERAL (N_("Frame %1 encoded size was OK (%2)"), vf.index(), encoded->size());
				break;
			}

			LOG_GENERAL (N_("Frame %1 encoded size was small (%2); adding noise at level %3 with pixel skip %4"), vf.index(), encoded->size(), noise_amount, pixel_skip);

			/* The JPEG2000 is too low-bitrate for some decoders <cough>DSS200</cough> so add some noise
			 * and try again.  This is slow but hopefully won't happen too often.  We have to do
			 * convert_to_xyz() again because compress_j2k() corrupts its xyz parameter.
			 */

			xyz = convert_to_xyz (vf.frame(), boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2));
			auto size = xyz->size ();
			auto pixels = size.width * size.height;
			dcpomatic::RNG rng(42);
			for (auto c = 0; c < 3; ++c) {
				auto p = xyz->data(c);
				auto e = xyz->data(c) + pixels;
				while (p < e) {
					*p = std::min(4095, std::max(0, *p + (rng.get() % noise_amount)));
					p += pixel_skip;
				}
			}

			if (pixel_skip > 1) {
				--pixel_skip;
			} else {
				++noise_amount;
			}
			/* Something's gone badly wrong if this much noise doesn't help */
			DCPOMATIC_ASSERT (noise_amount < 16);
		}

		switch (vf.eyes()) {
			case Eyes::BOTH:
				LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for mono"), vf.index());
				break;
			case Eyes::LEFT:
				LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for L"), vf.index());
				break;
			case Eyes::RIGHT:
				LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for R"), vf.index());
				break;
			default:
				break;
		}

		LOG_TIMING("finish-local-encode thread=%1 frame=%2", thread_id(), vf.index());
	} catch (std::exception& e) {
		/* This is very bad, so don't cope with it, just pass it on */
		LOG_ERROR(N_("Local encode failed (%1)"), e.what());
		throw;
	}

	return encoded;
}


void
CPUJ2KFrameEncoder::log_thread_start ()
{
        LOG_TIMING("start-encoder-thread thread=%1 server=localhost", thread_id());
}


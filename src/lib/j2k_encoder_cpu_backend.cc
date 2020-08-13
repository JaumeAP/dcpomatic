#include "config.h"
#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "j2k_encoder_cpu_backend.h"
#include "player_video.h"
#include <dcp/j2k.h>

#include "i18n.h"


using std::string;
using boost::optional;
using boost::shared_ptr;
using dcp::Data;


optional<Data>
J2KEncoderCPUBackend::encode (shared_ptr<DCPVideo> video)
{
	try {
		LOG_TIMING ("start-local-encode thread=%1 frame=%2", thread_id(), video->index());
		string const comment = Config::instance()->dcp_j2k_comment();

		Data enc = dcp::compress_j2k (
			DCPVideo::convert_to_xyz (video->frame(), boost::bind(&Log::dcp_log, dcpomatic_log.get(), _1, _2)),
			video->j2k_bandwidth(),
			video->frames_per_second(),
			video->frame()->eyes() == EYES_LEFT || video->frame()->eyes() == EYES_RIGHT,
			video->resolution() == RESOLUTION_4K,
			comment.empty() ? "libdcp" : comment
			);

		switch (video->frame()->eyes()) {
		case EYES_BOTH:
			LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for mono"), video->index());
			break;
		case EYES_LEFT:
			LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for L"), video->index());
			break;
		case EYES_RIGHT:
			LOG_DEBUG_ENCODE (N_("Finished locally-encoded frame %1 for R"), video->index());
			break;
		default:
			break;
		}

		LOG_TIMING ("finish-local-encode thread=%1 frame=%2", thread_id(), video->index());
		return enc;
	} catch (std::exception& e) {
		/* This is very bad, so don't cope with it, just pass it on */
		LOG_ERROR (N_("Local encode failed (%1)"), e.what());
		throw;
	}
}


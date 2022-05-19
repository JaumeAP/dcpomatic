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


#include "cpu_j2k_frame_encoder.h"
#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"

#include "i18n.h"


using std::shared_ptr;
using boost::optional;
using dcp::ArrayData;


optional<ArrayData>
CPUJ2KFrameEncoder::encode (DCPVideo const& vf)
{
	optional<ArrayData> encoded;

	try {
		LOG_TIMING("start-local-encode thread=%1 frame=%2", thread_id(), vf.index());
		encoded = vf.encode_locally();
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


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


#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "remote_j2k_frame_encoder.h"

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;
using boost::optional;
using dcp::Data;


optional<dcp::ArrayData>
RemoteJ2KFrameEncoder::encode(DCPVideo const& vf)
{
	optional<dcp::ArrayData> encoded;

	try {
		encoded = vf.encode_remotely(_server);

		if (_remote_backoff > 0) {
			LOG_GENERAL ("%1 was lost, but now she is found; removing backoff", _server.host_name());
		}

		/* This job succeeded, so remove any backoff */
		_remote_backoff = 0;

	} catch (std::exception& e) {
		if (_remote_backoff < 60) {
			/* back off more */
			_remote_backoff += 10;
		}
		LOG_ERROR (
			N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
			vf.index(), _server.host_name(), e.what(), _remote_backoff
			);
	}

	return encoded;
}


void
RemoteJ2KFrameEncoder::log_thread_start ()
{
       LOG_TIMING("start-encoder-thread thread=%1 server=%2", thread_id(), _server.host_name());
}


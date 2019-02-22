/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "remote_j2k_encode_worker.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "cross.h"

#include "i18n.h"

using dcp::Data;
using boost::optional;
using boost::shared_ptr;

optional<Data>
RemoteJ2KEncodeWorker::encode (shared_ptr<DCPVideo> vf)
{
	optional<Data> encoded;

	try {
		encoded = vf->encode_remotely (_server);

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
			vf->index(), _server.host_name(), e.what(), _remote_backoff
			);
	}

	return encoded;
}

void
RemoteJ2KEncodeWorker::log_thread_start ()
{
	LOG_TIMING ("start-encoder-thread thread=%1 server=%2", thread_id(), _server.host_name());
}

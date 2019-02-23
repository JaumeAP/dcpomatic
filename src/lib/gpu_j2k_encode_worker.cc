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

#include "gpu_j2k_encode_worker.h"
#include "dcp_video.h"
#include "cross.h"
#include "dcpomatic_log.h"

#include "i18n.h"

using dcp::Data;
using boost::optional;
using boost::shared_ptr;

GPUJ2KEncodeWorker::GPUJ2KEncodeWorker ()
{
	/* XXX: setup poznan library */
}

optional<Data>
GPUJ2KEncodeWorker::encode (shared_ptr<DCPVideo> vf)
{
	optional<Data> encoded;

	return encoded;
}

void
GPUJ2KEncodeWorker::log_thread_start ()
{
	LOG_TIMING ("start-encoder-thread thread=%1 GPU", thread_id());
}

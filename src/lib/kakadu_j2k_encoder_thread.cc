/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "j2k_encoder.h"
#include "kakadu_j2k_encoder_thread.h"
#include "util.h"
#include <boost/interprocess/sync/scoped_lock.hpp>

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;


KakaduJ2KEncoderThread::KakaduJ2KEncoderThread(J2KEncoder& encoder)
	: J2KSyncEncoderThread(encoder)
{

}


void
KakaduJ2KEncoderThread::log_thread_start() const
{
	start_of_thread("KakaduJ2KEncoder");
	LOG_TIMING("start-encoder-thread thread={} server=localhost", thread_id());
}


shared_ptr<dcp::ArrayData>
KakaduJ2KEncoderThread::encode(DCPVideo const& frame)
{
	auto shmem = _encoder.kakadu_shared_memory();
	if (!shmem) {
		LOG_ERROR(N_("Kakadu encode failed (no shared memory)"));
		return {};
	}

	auto metadata = shmem->metadata();

	std::cout << "Send an image.\n";
	boost::interprocess::scoped_lock lm(metadata->mutex);
	frame.convert_to_xyz(shmem->xyz());

	metadata->request = true;
	metadata->request_condition.notify_one();

	while (!metadata->reply) {
		metadata->reply_condition.wait(lm);
	}
	metadata->reply = false;
	std::cout << "reading " << metadata->j2k_size << "\n";

	return make_shared<dcp::ArrayData>(shmem->j2k(), metadata->j2k_size);
}


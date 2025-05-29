/*
    Copyright (C) 2023 Grok Image Compression Inc.

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


#include "messenger.h"


using namespace grk_plugin;



Synch::Synch(char const* sent_sem_name, char const* receive_ready_sem_name)
	: _sent_sem_name(sent_sem_name)
	, _receive_ready_sem_name(receive_ready_sem_name)
{
	// unlink semaphores in case of previous crash
	unlink();
	open();
}


Synch::~Synch()
{
	close();
	unlink();
}


void
Synch::post(SynchDirection dir)
{
	auto sem = dir == SYNCH_SENT ? _sent_sem : _receive_ready_sem;
	if (sem_post(sem)) {
		getMessengerLogger()->error("Error posting to semaphore: %s", strerror(errno));
	}
}

void
Synch::wait(SynchDirection dir)
{
	auto sem = dir == SYNCH_SENT ? _sent_sem : _receive_ready_sem;
	if (sem_wait(sem)) {
		getMessengerLogger()->error("Error waiting for semaphore: %s", strerror(errno));
	}
}


void
Synch::open()
{
	_sent_sem = sem_open(_sent_sem_name.c_str(), O_CREAT, 0666, 0);
	if (!_sent_sem) {
		getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
	}

	_receive_ready_sem = sem_open(_receive_ready_sem_name.c_str(), O_CREAT, 0666, 1);
	if (!_receive_ready_sem) {
		getMessengerLogger()->error("Error opening shared memory: %s", strerror(errno));
	}
}


void
Synch::close()
{
	if (sem_close(_sent_sem)) {
		getMessengerLogger()->error("Error closing semaphore %s: %s", _sent_sem_name.c_str(), strerror(errno));
	}

	if (sem_close(_receive_ready_sem)) {
		getMessengerLogger()->error("Error closing semaphore %s: %s", _receive_ready_sem_name.c_str(), strerror(errno));
	}
}


void
Synch::unlink()
{
	int rc = sem_unlink(_sent_sem_name.c_str());
	if (rc == -1 && errno != ENOENT) {
		getMessengerLogger()->error("Error unlinking semaphore %s: %s", _sent_sem_name.c_str(), strerror(errno));
	}
	rc = sem_unlink(_receive_ready_sem_name.c_str());
	if (rc == -1 && errno != ENOENT) {
		getMessengerLogger()->error("Error unlinking semaphore %s: %s", _receive_ready_sem_name.c_str(), strerror(errno));
	}
}



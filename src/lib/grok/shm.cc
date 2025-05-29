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


#include "context.h"
#include "messenger.h"
#include "shm.h"


bool
grk_plugin::init_shm(char const* name, size_t len, int* shm_fd, char** buffer)
{
	*shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666);

	if (*shm_fd < 0) {
		getGrokLogger()->error("Error opening shared memory: %s", strerror(errno));
		return false;
	}

	int rc = ftruncate(*shm_fd, sizeof(char) * len);

	if (rc) {
		getGrokLogger()->error("Error truncating shared memory: %s", strerror(errno));
		if (close(*shm_fd)) {
			getGrokLogger()->error("Error closing shared memory: %s", strerror(errno));
		}
		rc = shm_unlink(name);
		if (rc && errno != ENOENT) {
			getGrokLogger()->error("Error unlinking shared memory: %s", strerror(errno));
		}
		return false;
	}

	*buffer = static_cast<char*>(mmap(0, len, PROT_WRITE, MAP_SHARED, *shm_fd, 0));

	if (!*buffer) {
		getGrokLogger()->error("Error mapping shared memory: %s", strerror(errno));
		if (close(*shm_fd)) {
			getGrokLogger()->error("Error closing shared memory: %s", strerror(errno));
		}
		rc = shm_unlink(name);
		if (rc && errno != ENOENT) {
			getGrokLogger()->error("Error unlinking shared memory: %s", strerror(errno));
		}
	}

	return *buffer != nullptr;
}


bool
grk_plugin::deinit_shm(char const* name, size_t len, int &shm_fd, char** buffer)
{
	if (!*buffer || !shm_fd) {
		return true;
	}

	int rc = munmap(*buffer, len);
	*buffer = nullptr;

	if (rc) {
		getGrokLogger()->error("Error unmapping shared memory %s: %s", name, strerror(errno));
	}

	rc = close(shm_fd);
	shm_fd = 0;

	if (rc) {
		getGrokLogger()->error("Error closing shared memory %s: %s", name, strerror(errno));
	}

	rc = shm_unlink(name);

	if (rc && errno != ENOENT) {
		getGrokLogger()->error("Error unlinking shared memory %s : %s", name, strerror(errno));
	}

	return true;
}



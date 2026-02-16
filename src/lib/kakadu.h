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


#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>


struct KakaduSharedMetadata
{
	boost::interprocess::interprocess_mutex mutex;
	bool request = false;
	boost::interprocess::interprocess_condition request_condition;
	bool reply = false;
	int j2k_size = 0;
	boost::interprocess::interprocess_condition reply_condition;
};



class KakaduSharedMemory
{
public:
	KakaduSharedMemory(int index);
	~KakaduSharedMemory();

	KakaduSharedMemory(KakaduSharedMemory const&) = delete;
	KakaduSharedMemory& operator=(KakaduSharedMemory const&) = delete;

	KakaduSharedMetadata* metadata();
	uint16_t* xyz();
	uint8_t* j2k();

private:
	boost::interprocess::managed_shared_memory _segment;
};


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


#include <semaphore.h>
#include <string>


namespace grk_plugin {


enum SynchDirection
{
	SYNCH_SENT,
	SYNCH_RECEIVE_READY
};


class Synch
{
public:
	Synch(char const* sent_sem_name, char const* receive_ready_sem_name);
	~Synch();

	void post(SynchDirection dir);
	void wait(SynchDirection dir);

	void open();
	void close();

	void unlink();

private:
	sem_t* _sent_sem;
	sem_t* _receive_ready_sem;

	std::string _sent_sem_name;
	std::string _receive_ready_sem_name;
};


}


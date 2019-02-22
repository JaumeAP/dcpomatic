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

#include "j2k_encode_worker.h"
#include "encode_server_description.h"
#include <dcp/data.h>

class DCPVideo;

class RemoteJ2KEncodeWorker : public J2KEncodeWorker
{
public:
	RemoteJ2KEncodeWorker (EncodeServerDescription s)
		: _server(s)
		, _remote_backoff(0)
	{}

	boost::optional<dcp::Data> encode (boost::shared_ptr<DCPVideo>);
	void log_thread_start ();

private:
	EncodeServerDescription _server;
	int _remote_backoff;
};

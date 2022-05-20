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


#ifndef DCPOMATIC_REMOTE_J2K_FRAME_ENCODER
#define DCPOMATIC_REMOTE_J2K_FRAME_ENCODER


#include "j2k_frame_encoder.h"


class RemoteJ2KFrameEncoder : public J2KFrameEncoder
{
public:
	RemoteJ2KFrameEncoder(EncodeServerDescription s, int timeout = 30)
		: _server(s)
		, _timeout(timeout)
	{}

	boost::optional<dcp::ArrayData> encode (DCPVideo const &) override;
	void log_thread_start () override;

private:
	EncodeServerDescription _server;
	int _timeout;
	int _remote_backoff = 0;
};


#endif


/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_J2K_ENCODER_REMOTE_BACKEND_H
#define DCPOMATIC_J2K_ENCODER_REMOTE_BACKEND_H


#include "encode_server_description.h"
#include "j2k_encoder_backend.h"


class J2KEncoderRemoteBackend : public J2KEncoderBackend
{
public:
	J2KEncoderRemoteBackend (EncodeServerDescription server, int timeout = 30)
		: _server (server)
		, _timeout (timeout)
	{}

	J2KEncoderRemoteBackend (J2KEncoderRemoteBackend&& other);

	std::vector<dcp::ArrayData> encode (std::vector<DCPVideo> const& video) override;

private:
	EncodeServerDescription _server;
	int _timeout;
	int _backoff = 0;
};


#endif


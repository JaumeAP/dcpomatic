/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_UPLOAD_DESTINATION_H
#define DCPOMATIC_UPLOAD_DESTINATION_H


#include "types.h"
#include <libcxml/cxml.h>


class UploadDestination
{
public:
	UploadDestination(
		std::string name_,
		FileTransferProtocol protocol_,
		bool passive_ftp_,
		boost::optional<int> maximum_connections_,
		std::string host_,
		std::string path_,
		std::string user_,
		std::string password_
	)
		: name(name_)
		, protocol(protocol_)
		, passive_ftp(passive_ftp_)
		, maximum_connections(maximum_connections_)
		, host(host_)
		, path(path_)
		, user(user_)
		, password(password_)
	{}

	UploadDestination(cxml::ConstNodePtr node);

	void as_xml(xmlpp::Element* parent) const;

	std::string name;
	FileTransferProtocol protocol;
	bool passive_ftp = true;
	boost::optional<int> maximum_connections;
	std::string host;
	std::string path;
	std::string user;
	std::string password;
};


bool operator==(UploadDestination const& a, UploadDestination const& b);


#endif


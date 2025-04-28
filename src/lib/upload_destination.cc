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


#include "upload_destination.h"
#include <libcxml/cxml.h>


UploadDestination::UploadDestination(cxml::ConstNodePtr node)
	: name(node->string_child("Name"))
	, protocol(node->string_child("Protocol") == "SCP" ? FileTransferProtocol::SCP : FileTransferProtocol::FTP)
	, passive_ftp(node->bool_child("PassiveFTP"))
	, host(node->string_child("Host"))
	, path(node->string_child("Path"))
	, user(node->string_child("User"))
	, password(node->string_child("Password"))
{

}


void
UploadDestination::as_xml(xmlpp::Element* parent) const
{
	cxml::add_text_child(parent, "Name", name);
	cxml::add_text_child(parent, "Protocol", protocol == FileTransferProtocol::SCP ? "SCP" : "FTP");
	cxml::add_text_child(parent, "PassiveFTP", passive_ftp ? "1" : "0");
	cxml::add_text_child(parent, "Host", host);
	cxml::add_text_child(parent, "Path", path);
	cxml::add_text_child(parent, "User", user);
	cxml::add_text_child(parent, "Password", password);
}


bool
operator==(UploadDestination const& a, UploadDestination const& b)
{
	return a.name == b.name
		&& a.protocol == b.protocol
		&& a.passive_ftp == b.passive_ftp
		&& a.host == b.host
		&& a.path == b.path
		&& a.user == b.user
		&& a.password == b.password;
}

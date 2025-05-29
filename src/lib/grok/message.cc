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
#include <string>


using std::string;
using namespace grk_plugin;


Message::Message(std::string const& msg)
{
	std::stringstream ss(msg);
	while (ss.good()) {
		std::string substr;
		std::getline(ss, substr, ',');
		_cs.push_back(substr);
	}
}

string
Message::next()
{
	if (_ct == _cs.size()) {
		getMessengerLogger()->error("Message: comma separated list exhausted. returning empty.");
		return "";
	}
	return _cs[_ct++];
}


uint32_t
Message::nextUint()
{
	return (uint32_t)std::stoi(next());
}



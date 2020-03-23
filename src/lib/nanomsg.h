/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include <string>
#include <list>
#include <boost/optional.hpp>
#include <boost/noncopyable.hpp>

class Nanomsg : public boost::noncopyable
{
public:
	explicit Nanomsg (bool server);

	void send (std::string s);
	std::string blocking_get ();
	boost::optional<std::string> nonblocking_get ();

private:
	boost::optional<std::string> get_from_pending ();
	void recv_and_parse (bool blocking);

	int _socket;
	std::list<std::string> _pending;
	std::string _current;
};


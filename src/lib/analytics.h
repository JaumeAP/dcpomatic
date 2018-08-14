/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "state.h"
#include "signaller.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/signals2.hpp>
#include <string>
#include <map>

class Job;

class Event
{
public:
	Event ();
	Event (cxml::ConstNodePtr node);

	void set (std::string k, std::string v);
	std::string get (std::string k) const;

	void as_xml (xmlpp::Element* parent) const;
	std::string dump () const;

private:
	struct timeval _time;
	std::map<std::string, std::string> _data;
};

class Analytics : public State, public Signaller
{
public:
	Analytics ();

	void job_state_changed (boost::shared_ptr<Job> job);

	void write () const;
	void read ();

	boost::signals2::signal<void (std::string, std::string)> Message;

	static Analytics* instance ();

private:
	int successful_dcp_encodes () const;

	/** Mutex to protect _id and _events */
	mutable boost::mutex _mutex;
	std::string _id;
	std::list<Event> _events;
	static Analytics* _instance;
	static int const _current_version;
};

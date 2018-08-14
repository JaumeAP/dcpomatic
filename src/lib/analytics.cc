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

#include "analytics.h"
#include "exceptions.h"
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include "i18n.h"

using std::string;
using dcp::raw_convert;
using boost::algorithm::trim;

Analytics* Analytics::_instance;
int const Analytics::_current_version = 1;

Event::Event (cxml::ConstNodePtr node)
{
	_time.tv_sec = node->number_child<int64_t>("Time");
	_time.tv_usec = 0;
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children()) {
		set(i->name(), i->content());
	}
}

void
Event::set (string k, string v)
{
	_data[k] = v;
}

void
Event::as_xml (cxml::NodePtr parent)
{
	/* It would be nice if this had timezone */
	parent->add_child("Time")->add_child_text(raw_convert<int64_t>(_time.tv_sec));
	for (map<string, string>::const_iterator i = _data.begin(); i != _data.end(); ++i) {
		parent->add_child(i->first)->add_child_text(i->second);
	}
}

Analytics::Analytics ()
{

}

int
Analytics::successful_dcp_encodes () const
{
	boost::mutex::scoped_lock lm (_mutex);
	BOOST_FOREACH (Event e, _events) {
		cout << e.dump() << "\n";
	}
}

void
Analytics::job_state_changed (shared_ptr<Job> job)
{
	Event ev;
	ev.set ("type", "job-state");
	ev.set ("json_name", job->json_name());
	ev.set ("sub_name", job->sub_name());
	ev.set ("error-summary", job->error_summary());
	ev.set ("error-details", job->error_details());
	ev.set ("status", job->json_status());

	{
		boost::mutex::scoped_lock lm (_mutex);
		_events.push_back (ev);
	}

	write ();

	if (successful_dcp_encodes() == 3) {
		emit (
			boost::bind(
				boost::ref(Message),
				_("Congratulations!"),
				_(
					"<h2>You have made 3 DCPs with DCP-o-matic!</h2>"
					"<img width=\"20%\" src=\"memory:me.jpg\" align=\"center\">"
                                        "<p>Hello. I'm Carl and I'm the "
					"developer of DCP-o-matic. I work on it in my spare time (with the help "
					"of a fine volunteer team of testers and translators) and I release it "
					"as free software."

					"<p>If you find DCP-o-matic useful, please consider a donation to the "
					"project. Financial support will help me to spend more "
					"time developing DCP-o-matic and making it better!"

					"<p><ul>"
					"<li><a href=\"https://dcpomatic.com/donate_amount?amount=40\">Go to Paypal to donate £40</a>"
					"<li><a href=\"https://dcpomatic.com/donate_amount?amount=20\">Go to Paypal to donate £20</a>"
					"<li><a href=\"https://dcpomatic.com/donate_amount?amount=10\">Go to Paypal to donate £10</a>"
					"</ul>"

					"<p>Thank you!"
					)
				)
			);
	}
}

void
Analytics::write () const
{
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Analytics");

	root->add_child("Version")->add_child_text(raw_convert<string>(_current_version));
	boost::mutex::scoped_lock lm (_mutex);
	BOOST_FOREACH (Event e, _events) {
		e.as_xml (root->add_child("Event"));
	}

	try {
		doc.write_to_file_formatted(path("analytics.xml").string());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, path("analytics.xml"));
	}
}

void
Analytics::read ()
try
{
	cxml::Document f ("Analytics");
	f.read_file (path("analytics.xml"));
	boost::mutex::scoped_lock lm (_mutex);
	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("Event")) {
		_events.push_back (Event(i));
	}
} catch (...) {
	/* Never mind */
}

Analytics*
Analytics::instance ()
{
	if (!_instance) {
		_instance = new Analytics();
		_instance->read();
	}

	return _instance;
}

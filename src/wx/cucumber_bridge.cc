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


#include "cucumber_bridge.h"
#include "lib/nanomsg.h"
#include "lib/dcpomatic_assert.h"
#include <dcp/raw_convert.h>
#include <boost/ref.hpp>
#include <iostream>


using std::string;
using boost::optional;
using boost::bind;


CucumberBridge::CucumberBridge()
	: _nanomsg(true, "cucumber")
{

}


void
CucumberBridge::listener()
{
	if (!_nanomsg.send(CUCUMBER_BRIDGE_READY "\n", 200)) {
		std::cerr << "Could not say hello to cucumber server.\n";
	}

	while (true) {
		auto id = _nanomsg.receive(-1);
		if (!id) {
			continue;
		}

		Message message;
		message.id = *id;

		if (
			*id == CUCUMBER_BRIDGE_CLICK_REGISTERED_BUTTON ||
			*id == CUCUMBER_BRIDGE_CLICK_BUTTON_ID ||
			*id == CUCUMBER_BRIDGE_SELECT_MENU ||
			*id == CUCUMBER_BRIDGE_TYPE ||
			*id == CUCUMBER_BRIDGE_ADD_CONTENT_FILE
		   ) {
			auto p = _nanomsg.receive(100);
			DCPOMATIC_ASSERT(p);
			message.parameter = *p;
		}

		boost::mutex::scoped_lock lm(_mutex);
		_queue.push_back(message);
	}
}


void
CucumberBridge::start()
{
	wxTheApp->Bind(wxEVT_IDLE, boost::bind(&CucumberBridge::idle, this));
	_thread = boost::thread(&CucumberBridge::listener, this);
}


void
CucumberBridge::idle()
{
	boost::mutex::scoped_lock lm(_mutex);
	if (_queue.empty()) {
		return;
	}

	auto message = _queue.front();
	_queue.pop_front();
	lm.unlock();

	if (message.id == CUCUMBER_BRIDGE_CLICK_REGISTERED_BUTTON) {
		ClickRegisteredButton(*message.parameter);
	} else if (message.id == CUCUMBER_BRIDGE_CLICK_BUTTON_ID) {
		ClickButtonId(static_cast<wxStandardID>(dcp::raw_convert<int>(*message.parameter)));
	} else if (message.id == CUCUMBER_BRIDGE_SELECT_MENU) {
		SelectMenu(*message.parameter);
	} else if (message.id == CUCUMBER_BRIDGE_TYPE) {
		Type(*message.parameter);
	} else if (message.id == CUCUMBER_BRIDGE_ADD_CONTENT_FILE) {
		AddContentFile(*message.parameter);
	} else if (message.id == CUCUMBER_BRIDGE_GET_CONTENT_LIST) {
		/* I hope it's OK to send and receive from nanomsg sockets from different threads */
		_nanomsg.send(GetContentList()->c_str(), -1);
	}
}

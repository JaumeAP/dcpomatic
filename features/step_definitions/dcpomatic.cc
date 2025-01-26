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


#include "wx/cucumber_bridge.h"
#include "lib/nanomsg.h"
#include <fmt/format.h>
#include <boost/test/unit_test.hpp>

#include <cucumber-cpp/autodetect.hpp>


using std::getline;
using std::string;
using std::stringstream;
using std::vector;
using cucumber::ScenarioScope;


auto constexpr TIMEOUT = 200;


struct Context
{
	Context()
		: _nanomsg(false, "cucumber")
	{

	}

	void click_button(string id)
	{
		general_delay();
		send(CUCUMBER_BRIDGE_CLICK_REGISTERED_BUTTON);
		send(id);
	}

	/* Click a button in the focussed dialog */
	void click_button(wxStandardID id)
	{
		general_delay();
		send(CUCUMBER_BRIDGE_CLICK_BUTTON_ID);
		send(fmt::to_string(id));
	}

	void select_menu(string id)
	{
		general_delay();
		send(CUCUMBER_BRIDGE_SELECT_MENU);
		send(id);
	}

	void type(std::string text)
	{
		general_delay();
		send(CUCUMBER_BRIDGE_TYPE);
		send(text);
	}

	void add_content_file(string filename)
	{
		general_delay();
		send(CUCUMBER_BRIDGE_ADD_CONTENT_FILE);
		send(filename);
	}

	vector<string> content_list()
	{
		general_delay();
		send(CUCUMBER_BRIDGE_GET_CONTENT_LIST);
		stringstream all(receive());
		vector<string> split;
		string one;
		while (getline(all, one, '\n')) {
			split.push_back(one);
		}
		return split;
	}

private:
	void send(string m)
	{
		BOOST_REQUIRE(_nanomsg.send(m + "\n", TIMEOUT));
	}

	std::string receive()
	{
		auto reply = _nanomsg.receive(TIMEOUT);
		BOOST_REQUIRE(static_cast<bool>(reply));
		return *reply;
	}

	void general_delay()
	{
		sleep(2);
	}

	Nanomsg _nanomsg;
};


GIVEN("^I have created a new film$")
{
	ScenarioScope<Context> context;
	context->select_menu(CUCUMBER_MENU_FILE_NEW);
	context->type("test");
	context->click_button(wxID_OK);
}


WHEN("^I add file (.+) as content$")
{
	REGEX_PARAM(string, filename);
	ScenarioScope<Context> context;
	/* I could not find a way to reliably enter text into a open file dialogue, so
	 * there's a custom method here.
	 */
	context->add_content_file(filename);
}

THEN("^the result should be only (.+) in the content list$")
{
	REGEX_PARAM(string, entry);
	ScenarioScope<Context> context;
	auto content = context->content_list();
	BOOST_REQUIRE_EQUAL(content.size(), 1);
	BOOST_CHECK_EQUAL(content.front(), entry);
}


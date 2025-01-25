/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include <boost/test/unit_test.hpp>
#include <cucumber-cpp/autodetect.hpp>


using std::string;
using cucumber::ScenarioScope;


class Context
{
public:
	void select_menu_item(string id)
	{
		std::cout << "select_menu_item " << id << "\n";
	}
};



GIVEN("^I have created a new film$")
{
	ScenarioScope<Context> context;
	context->select_menu_item("File/New");
}


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


#include "lib/email.h"
#include <dcp/scope_guard.h>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>
#include <iostream>
#include <string>
#include <vector>


using std::string;
using std::vector;


/* Make sure email dates are always in English (per RFC 2822) */
BOOST_AUTO_TEST_CASE(email_date_format_test)
{
	auto previous = setlocale(LC_TIME, nullptr);
	dcp::ScopeGuard sg = [previous]() { setlocale(LC_TIME, previous); };

	auto result = setlocale(LC_TIME, "de_DE.UTF8");
	BOOST_REQUIRE(result);

	Email email("bob@popcornwiththat.com", { "orders@popcorn.com" }, "Popcorn order", "Please send popcorn.");
	email.create(0);

	vector<string> lines;
	boost::algorithm::split(lines, email.email(), boost::is_any_of("\n\r"));

	for (auto line: lines) {
		if (boost::starts_with(line, "Date: ")) {
			BOOST_CHECK_EQUAL(line, "Date: Thu, 01 Jan 1970 01:00:00 +0100");
		}
	}
}


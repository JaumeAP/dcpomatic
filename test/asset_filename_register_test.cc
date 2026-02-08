/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#include "lib/asset_filename_register.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(asset_filename_register_test)
{
	AssetFilenameRegister reg;
	BOOST_CHECK_EQUAL(reg.destination_filename("foo.mxf", "123").string(), "foo.mxf");
	BOOST_CHECK_EQUAL(reg.destination_filename("foo.mxf", "456").string(), "foo_0.mxf");
	/* We already used this file for ID 456 so we can just use it again */
	BOOST_CHECK_EQUAL(reg.destination_filename("foo.mxf", "456").string(), "foo_0.mxf");
	BOOST_CHECK_EQUAL(reg.destination_filename("bar.mxf", "123").string(), "bar.mxf");
	BOOST_CHECK_EQUAL(reg.destination_filename("bar.mxf", "789").string(), "bar_0.mxf");
	BOOST_CHECK_EQUAL(reg.destination_filename("bar.mxf", "1000").string(), "bar_1.mxf");
}



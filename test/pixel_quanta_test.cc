/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "lib/pixel_quanta.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(pixel_quanta_test)
{
	BOOST_CHECK(PixelQuanta(1, 1).round(9.3, 5.9) == dcp::Size(9, 6));
	BOOST_CHECK(PixelQuanta(1, 1).round(11.7, 5.1) == dcp::Size(12, 5));
	BOOST_CHECK(PixelQuanta(2, 2).round(9.3, 5.9) == dcp::Size(10, 6));
	BOOST_CHECK(PixelQuanta(2, 2).round(8.9, 11.9) == dcp::Size(8, 12));

}


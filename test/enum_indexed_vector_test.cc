/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "lib/enum_indexed_vector.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(enum_indexed_vector_referencing_test)
{
	enum class Foo {
		ONE,
		TWO,
		THREE,
		COUNT
	};

	EnumIndexedVector<int, Foo> vector;
	vector[Foo::ONE] = 1;
	vector[Foo::TWO] = 2;
	vector[Foo::THREE] = 3;

	for (auto i: vector) {
		++i;
	}

	BOOST_CHECK_EQUAL(vector[Foo::ONE], 1);
	BOOST_CHECK_EQUAL(vector[Foo::TWO], 2);
	BOOST_CHECK_EQUAL(vector[Foo::THREE], 3);

	for (auto& i: vector) {
		++i;
	}

	BOOST_CHECK_EQUAL(vector[Foo::ONE], 2);
	BOOST_CHECK_EQUAL(vector[Foo::TWO], 3);
	BOOST_CHECK_EQUAL(vector[Foo::THREE], 4);
}


BOOST_AUTO_TEST_CASE(enum_indexed_vector_indices_test)
{
	enum class Foo {
		ONE,
		TWO,
		THREE,
		COUNT
	};

	EnumIndexedVector<int, Foo> vector;

	auto const ref = std::vector<Foo>{Foo::ONE, Foo::TWO, Foo::THREE};
	BOOST_CHECK(vector.indices() == ref);

	// Make sure nothing funny happens on the second call to indices
	BOOST_CHECK(vector.indices() == ref);
}

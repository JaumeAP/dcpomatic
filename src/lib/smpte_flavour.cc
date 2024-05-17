/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_assert.h"
#include "smpte_flavour.h"
#include <string>


dcp::SMPTEFlavour
string_to_smpte_flavour(std::string const& s)
{
	if (s == "a") {
		return dcp::SMPTEFlavour::A;
	} else if (s == "bv20") {
		return dcp::SMPTEFlavour::BV20;
	} else if (s == "bv21") {
		return dcp::SMPTEFlavour::BV21;
	}

	DCPOMATIC_ASSERT(false);
}


std::string
smpte_flavour_to_string(dcp::SMPTEFlavour f)
{
	switch (f) {
	case dcp::SMPTEFlavour::A:
		return "a";
	case dcp::SMPTEFlavour::BV20:
		return "bv20";
	case dcp::SMPTEFlavour::BV21:
		return "bv21";
	}

	DCPOMATIC_ASSERT(false);
}


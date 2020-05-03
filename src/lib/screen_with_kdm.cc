/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "screen_with_kdm.h"
#include "cinema.h"
#include "screen.h"
#include "util.h"
#include <boost/foreach.hpp>

using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;

void
ScreenWithKDM::add_name_values (dcp::NameFormat::Map& values)
{
	values['c'] = screen->cinema ? screen->cinema->name : "";
	values['s'] = screen->name;
	values['i'] = kdm_id ();
}


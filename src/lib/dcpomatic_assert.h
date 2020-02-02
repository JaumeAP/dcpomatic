/*
    Copyright (C) 2014-2020 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_ASSERT_H
#define DCPOMATIC_ASSERT_H

#include "exceptions.h"
#include <boost/stacktrace.hpp>
#include <boost/exception/all.hpp>

typedef boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace> traced;

template <class E>
void throw_with_trace(E const& e)
{
	throw boost::enable_error_info(e) << traced(boost::stacktrace::stacktrace());
}

#define DCPOMATIC_ASSERT(x) if (!(x)) throw_with_trace(ProgrammingError (__FILE__, __LINE__));
#define DCPOMATIC_ASSERT_MESSAGE(x, m, ...) if (!(x)) throw_with_trace(ProgrammingError(__FILE__, __LINE__, String::compose(m, __VA_ARGS__)));

#endif

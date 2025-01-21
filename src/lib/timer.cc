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


/** @file src/timer.cc
 *  @brief Some timing classes for debugging and profiling.
 */


#include "compose.hpp"
#include "timer.h"
#include "util.h"
#include <fmt/format.h>
#include <sys/time.h>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::list;
using std::max;
using std::pair;
using std::string;
using boost::optional;


GlobalTimer* GlobalTimer::_instance = nullptr;


/** @param n Name to use when giving output */
PeriodTimer::PeriodTimer (string n)
	: _name (n)
{
	gettimeofday (&_start, 0);
}


/** Destroy PeriodTimer and output the time elapsed since its construction */
PeriodTimer::~PeriodTimer ()
{
	struct timeval stop;
	gettimeofday (&stop, 0);
	cout << fmt::format("T: {}: {:.3f}\n", _name, seconds(stop) - seconds(_start));
}


StateTimer::StateTimer (string n, string s)
	: _name (n)
{
	struct timeval t;
	gettimeofday (&t, 0);
	_time = seconds (t);
	_state = s;
}


StateTimer::StateTimer (string n)
	: _name (n)
{

}


void
StateTimer::set (string s)
{
	set_internal (s);
}


void
StateTimer::set_internal (optional<string> s)
{
	double const last = _time;
	struct timeval t;
	gettimeofday (&t, 0);
	_time = seconds (t);

	if (s && _counts.find(*s) == _counts.end()) {
		_counts[*s] = Counts();
	}

	if (_state) {
		_counts[*_state].total_time += _time - last;
		_counts[*_state].number++;
	}
	_state = s;
}


void
StateTimer::unset ()
{
	set_internal (optional<string>());
}


/** Destroy StateTimer and generate a summary of the state timings on cout */
StateTimer::~StateTimer ()
{
	if (!_state) {
		return;
	}

	unset ();

	int longest = 0;
	for (auto const& i: _counts) {
		longest = max (longest, int(i.first.length()));
	}

	list<pair<double, string>> sorted;

	for (auto const& i: _counts) {
		string name = i.first + string(longest + 1 - i.first.size(), ' ');
		char buffer[64];
		snprintf (buffer, 64, "%.4f", i.second.total_time);
		string total_time (buffer);
		sorted.push_back (make_pair(i.second.total_time, String::compose("\t%1%2 %3 %4", name, total_time, i.second.number, (i.second.total_time / i.second.number))));
	}

	sorted.sort ([](pair<double, string> const& a, pair<double, string> const& b) {
		return a.first > b.first;
	});


	cout << _name << N_(":\n");
	for (auto const& i: sorted) {
		cout << N_("\t") << i.second << "\n";
	}
}


GlobalTimer*
GlobalTimer::instance()
{
	if (!_instance) {
		_instance = new GlobalTimer();
	}

	return _instance;
}


void
GlobalTimer::reset()
{
	struct timeval now;
	gettimeofday(&now, 0);
	_reset = _last = seconds(now);
	// std::cout << "|----------------------\\\n";
}


void
GlobalTimer::mark(string const& tag)
{
	// std::cout << "|" << tag << " ";

	struct timeval now;
	gettimeofday(&now, 0);
	auto const now_seconds = seconds(now);

	if (_last > 0) {
		// std::cout << fmt::format("{:.2} ", now_seconds - _last);
	}

	if (_reset > 0) {
		// std::cout << fmt::format("{:.2} ", now_seconds - _reset);
	}

	_last = now_seconds;
	// std::cout << "\n";
}


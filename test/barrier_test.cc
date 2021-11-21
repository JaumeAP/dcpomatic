/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/barrier.h"
#include "lib/cross.h"
#include <boost/test/unit_test.hpp>
#include <boost/thread/thread.hpp>


BOOST_AUTO_TEST_CASE (barrier_test)
{
	int constexpr num_threads = 128;

	Barrier barrier (num_threads);

	boost::mutex mutex;
	int count = 0;

	auto thread_body = [&barrier, &mutex, &count]() {
		barrier.wait();
		boost::mutex::scoped_lock lm(mutex);
		++count;
	};

	std::vector<boost::thread*> threads;
	for (int i = 0; i < num_threads - 1; ++i) {
		threads.push_back(new boost::thread(thread_body));
	}

	dcpomatic_sleep_seconds(5);

	BOOST_CHECK_EQUAL (count, 0);

	threads.push_back(new boost::thread(thread_body));
	dcpomatic_sleep_seconds(5);

	BOOST_CHECK_EQUAL (count, num_threads);

	for (auto i: threads) {
		i->join();
		delete i;
	}
}


BOOST_AUTO_TEST_CASE (barrier_test_lower)
{
	int constexpr num_threads = 128;

	Barrier barrier (num_threads);

	boost::mutex mutex;
	int count = 0;

	auto thread_body = [&barrier, &mutex, &count]() {
		barrier.wait();
		boost::mutex::scoped_lock lm(mutex);
		++count;
	};

	std::vector<boost::thread*> threads;
	for (int i = 0; i < num_threads / 2; ++i) {
		threads.push_back(new boost::thread(thread_body));
	}

	dcpomatic_sleep_seconds(5);

	BOOST_CHECK_EQUAL (count, 0);

	barrier.lower();
	dcpomatic_sleep_seconds(5);

	BOOST_CHECK_EQUAL (count, num_threads / 2);

	for (auto i: threads) {
		i->join();
		delete i;
	}
}


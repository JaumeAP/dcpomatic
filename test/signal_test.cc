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


#include "lib/signal.h"
#include <boost/thread/condition.hpp>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(signal_test_basic)
{
	int signal_received = 0;

	Signal<void (int)> signal;
	signal.connect([&](int x) { signal_received = x;});
	signal.emit(42);

	BOOST_CHECK_EQUAL(signal_received, 42);
}


BOOST_AUTO_TEST_CASE(signal_test_disconnect)
{
	int signal_received = 0;

	Signal<void (int)> signal;
	auto connection = signal.connect([&](int x) { signal_received = x;});
	connection.disconnect();
	signal.emit(42);

	BOOST_CHECK_EQUAL(signal_received, 0);
}


BOOST_AUTO_TEST_CASE(signal_test_cross_thread)
{
	int signal_received = 0;

	Signal<void (int)> signal;
	signal.connect([&](int x) { signal_received = x; });

	boost::thread thread([&]() {
		signal.emit(42);
	});

	thread.join();

	BOOST_CHECK_EQUAL(signal_received, 0);
	pending::process();
	BOOST_CHECK_EQUAL(signal_received, 42);
}


BOOST_AUTO_TEST_CASE(signal_test_cross_thread2)
{
	int signal_received = 0;
	Signal<void (int)>* signal = nullptr;

	boost::mutex mutex;
	bool setup_done = false;
	boost::condition setup;
	bool emit_done = false;
	boost::condition emit;

	boost::thread thread([&]() {
		signal = new Signal<void (int)>();
		signal->connect([&](int x) { signal_received = x; });

		{
			boost::mutex::scoped_lock lm(mutex);
			setup_done = true;
			setup.notify_one();
		}

		{
			boost::mutex::scoped_lock lm(mutex);
			while (!emit_done) {
				emit.wait(lm);
			}
		}

		pending::process();
	});

	{
		boost::mutex::scoped_lock lm(mutex);
		while (!setup_done) {
			setup.wait(lm);
		}
	}

	signal->emit(42);

	{
		boost::mutex::scoped_lock lm(mutex);
		emit_done = true;
		emit.notify_one();
	}

	thread.join();

	BOOST_CHECK_EQUAL(signal_received, 42);
}


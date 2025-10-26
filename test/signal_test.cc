#include "lib/signal.h"
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/bind/bind.hpp>

using namespace boost::placeholders;


BOOST_AUTO_TEST_CASE(signal_test_basic_same_thread)
{
	int signal_received = 0;

	Signal<void (int)> signal;
	signal.connect_same_thread([&](int x) {
		signal_received = x;
	});

	signal(42);

	BOOST_CHECK_EQUAL(signal_received, 42);
}


BOOST_AUTO_TEST_CASE(signal_test_disconnect)
{
	int signal_received = 0;

	Signal<void (int)> signal;
	auto connection = signal.connect_same_thread([&](int x) { signal_received = x;});
	connection.disconnect();
	signal(42);

	BOOST_CHECK_EQUAL(signal_received, 0);
}


static
void
emit_signal_from_thread(Signal<void (int)>* signal)
{
	boost::mutex mutex;
	boost::condition emitted_condition;
	bool emitted = false;

	boost::thread thread([&]() {
		(*signal)(42);
		boost::mutex::scoped_lock lm(mutex);
		emitted = true;
		emitted_condition.notify_all();
	});

	boost::mutex::scoped_lock lm(mutex);
	while (!emitted) {
		emitted_condition.wait(lm);
	}

	thread.join();
}


BOOST_AUTO_TEST_CASE(signal_test_basic_worker_to_ui_thread)
{
	int signal_received = 0;

	Signal<void (int)> signal;
	signal.connect_ui_thread(nullptr, [&](int x) {
		BOOST_CHECK(boost::this_thread::get_id() == dcpomatic::signal::manager->ui_thread());
		signal_received = x;
	});

	emit_signal_from_thread(&signal);

	dcpomatic::signal::manager->process_pending();

	BOOST_CHECK_EQUAL(signal_received, 42);
}


BOOST_AUTO_TEST_CASE(signal_test_signal_going_away)
{
	int signal_received = 0;

	auto signal = new Signal<void (int)>();
	signal->connect_ui_thread(nullptr, [&](int x) {
		signal_received = x;
	});

	emit_signal_from_thread(signal);
	delete signal;

	dcpomatic::signal::manager->process_pending();

	BOOST_CHECK_EQUAL(signal_received, 42);
}


BOOST_AUTO_TEST_CASE(signal_test_disconnection_when_signal_gone_away)
{
	auto signal = new Signal<void (int)>();
	auto connection = signal->connect_same_thread([&](int) {});
	delete signal;
	connection.disconnect();
}


BOOST_AUTO_TEST_CASE(signal_test_handler_going_away)
{
	Signal<void (int)> signal;

	class A : public Trackable
	{
	public:
		void handler(int x)
		{
			_x += x;
		}

	private:
		int _x = 1;
	};

	auto a = new A;

	signal.connect_ui_thread(a, boost::bind(&A::handler, a, _1));

	emit_signal_from_thread(&signal);

	delete a;

	dcpomatic::signal::manager->process_pending();
}


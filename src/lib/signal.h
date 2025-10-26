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


#ifndef DCPOMATIC_SIGNAL_H
#define DCPOMATIC_SIGNAL_H


#include "cross.h"
#include <boost/thread.hpp>
#include <iostream>


class SignalBase
{
public:
	virtual ~SignalBase() = default;
	virtual void disconnect(int id) = 0;
};


/** @class Connection
 *  @brief Record of a connection to a signal, so it can be disconnected.
 */
class Connection
{
public:
	Connection(SignalBase& signal, int id)
		: _signal(signal)
		, _id(id)
	{}

	Connection& operator=(Connection const& other)
	{
		_signal = other._signal;
		_id = other._id;
		return *this;
	}

	void disconnect()
	{
		_signal.disconnect(_id);
	}

private:
	SignalBase& _signal;
	int _id;
};


/** @class ScopedConnection
 *  @brief Connection that disconnects itself on destruction.
 */
class ScopedConnection
{
public:
	ScopedConnection() {}

	ScopedConnection(Connection connection)
		: _connection(std::move(connection))
	{}

	ScopedConnection(ScopedConnection const& other) = delete;

	ScopedConnection(ScopedConnection&& other)
		: _connection(std::move(other._connection))
	{}

	ScopedConnection& operator=(ScopedConnection const& other) = delete;

	ScopedConnection& operator=(ScopedConnection&& other)
	{
		if (this != &other) {
			_connection = other._connection;
			other._connection.reset();
		}
		return *this;
	}

	~ScopedConnection()
	{
		if (_connection) {
			_connection->disconnect();
		}
	}

private:
	boost::optional<Connection> _connection;
};


namespace pending {

extern boost::mutex mutex;
extern std::map<boost::thread::id, std::list<std::function<void ()>>> callbacks;
extern void process();

};


class ThreadWaker
{
public:
	virtual void wake() {}
};


extern ThreadWaker* thread_waker;


template <typename Parameters>
class Signal : public SignalBase
{
private:
	struct Callback
	{
		int id;
		boost::thread::id thread;
		/* XXX */
		std::string thread_name;
		std::function<Parameters> function;
	};

public:
	Connection connect(std::function<Parameters> callback)
	{
		boost::mutex::scoped_lock lm(_mutex);
		auto const id = _id++;
		auto const thread_id = boost::this_thread::get_id();
		_callbacks.push_back(Callback{id, thread_id, thread_name(), callback});
		return Connection(*this, id);
	}

	void disconnect(int id) override
	{
		boost::mutex::scoped_lock lm(_mutex);
		auto iter = std::find_if(_callbacks.begin(), _callbacks.end(), [id](Callback const& callback) { return callback.id == id; });
		if (iter != _callbacks.end()) {
			_callbacks.erase(iter);
		}
	}

	template <typename... Args>
	void operator()(Args...args)
	{
		emit(args...);
	}

	template <typename... Args>
	void emit(Args... args)
	{
                boost::mutex::scoped_lock lm(_mutex);
                auto thread_id = boost::this_thread::get_id();
		std::cout << "emit with " << _callbacks.size() << "\n";
                for (auto const& callback: _callbacks) {
                        if (thread_id == callback.thread) {
				std::cout << "simple emit.\n";
                                callback.function(args...);
                        } else {
				std::cout << "x-thread emit from " << thread_name() << " to " << callback.thread_name << "\n";
                                boost::mutex::scoped_lock lm2(pending::mutex);
				pending::callbacks[callback.thread].push_back(boost::bind(callback.function, args...));
				if (thread_waker) {
					std::cout << "x-thread emit wakes shit.\n";
					thread_waker->wake();
				}
                        }
                }
	}

private:
	boost::mutex _mutex;
	int _id = 0;
        std::list<Callback> _callbacks;
};


#endif


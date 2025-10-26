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


#include "dcpomatic_assert.h"
#include <boost/optional.hpp>
#include <boost/thread.hpp>
#include <set>


class SignalBase;


class Trackable
{
public:
	~Trackable();
};


class SignalManager2
{
public:
	/** Must be called from the UI thread, as the current thread ID will be
	 *  taken as the UI one.
	 */
	SignalManager2();

	virtual ~SignalManager2() {}

	boost::thread::id ui_thread() const {
		return _ui_thread;
	}

	void add_pending(Trackable* trackable, std::function<void ()> pending);
	void process_pending();

	void register_signal(SignalBase* signal);
	void unregister_signal(SignalBase* signal);

	void register_trackable(Trackable* trackable);
	void unregister_trackable(Trackable* trackable);

	void disconnect(SignalBase* signal, int id);

	virtual void wake();

private:
	boost::thread::id _ui_thread;
	boost::mutex _pending_mutex;
	std::list<std::pair<Trackable*, std::function<void ()>>> _pending;
	boost::mutex _signals_mutex;
	std::set<SignalBase*> _signals;
	boost::mutex _trackables_mutex;
	std::set<Trackable*> _trackables;
};


namespace dcpomatic {
namespace signal {
	extern SignalManager2* manager;
}
}


class SignalBase
{
public:
	SignalBase()
	{
		DCPOMATIC_ASSERT(dcpomatic::signal::manager);
		dcpomatic::signal::manager->register_signal(this);
	}

	virtual ~SignalBase()
	{
		dcpomatic::signal::manager->unregister_signal(this);
	}

	virtual void disconnect(int id) = 0;
};


class Connection
{
public:
	Connection(SignalBase* signal, int id)
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
		DCPOMATIC_ASSERT(dcpomatic::signal::manager);
		dcpomatic::signal::manager->disconnect(_signal, _id);
	}

private:
	SignalBase* _signal;
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


template <class Signature>
class Signal : public SignalBase
{
private:
	struct Callback
	{
		int id;
		bool same_thread;
		Trackable* trackable = nullptr;
		std::function<Signature> function;
	};

public:
	Connection connect_same_thread(std::function<Signature> function)
	{
		boost::mutex::scoped_lock lm(_mutex);
		auto const id = _id++;
		_callbacks.push_back({id, true, nullptr, function});
		return Connection(this, id);
	}

	Connection connect_ui_thread(Trackable* trackable, std::function<Signature> function)
	{
		dcpomatic::signal::manager->register_trackable(trackable);
		boost::mutex::scoped_lock lm(_mutex);
		auto const id = _id++;
		_callbacks.push_back({id, false, trackable, function});
		return Connection(this, id);
	}

	template <typename... Args>
	void operator()(Args... args)
	{
		boost::mutex::scoped_lock lm(_mutex);
		DCPOMATIC_ASSERT(dcpomatic::signal::manager);
		bool in_ui_thread = boost::this_thread::get_id() == dcpomatic::signal::manager->ui_thread();
		for (auto const& callback: _callbacks) {
			if (callback.same_thread || in_ui_thread) {
				callback.function(args...);
			} else {
				dcpomatic::signal::manager->add_pending(callback.trackable, boost::bind(callback.function, args...));
			}
		}
	}

private:
	void disconnect(int id) override
	{
		boost::mutex::scoped_lock lm(_mutex);
		auto iter = std::find_if(_callbacks.begin(), _callbacks.end(), [id](Callback const& callback) { return callback.id == id; });
		if (iter != _callbacks.end()) {
			_callbacks.erase(iter);
		}
	}

	friend class Connection;

	boost::mutex _mutex;
	std::list<Callback> _callbacks;
	int _id = 0;
};


#endif


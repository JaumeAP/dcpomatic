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


#include "signal.h"

SignalManager2* dcpomatic::signal::manager = nullptr;


Trackable::~Trackable()
{
	dcpomatic::signal::manager->unregister_trackable(this);
}


SignalManager2::SignalManager2()
	: _ui_thread(boost::this_thread::get_id())
{

}


void
SignalManager2::add_pending(Trackable* trackable, std::function<void ()> pending)
{
	boost::mutex::scoped_lock lm(_pending_mutex);
	_pending.push_back(make_pair(trackable, pending));
	wake();
}


void
SignalManager2::process_pending()
{
	boost::mutex::scoped_lock lm(_pending_mutex);
	boost::mutex::scoped_lock lm2(_trackables_mutex);
	for (auto const& pending: _pending) {
		if (!pending.first || _trackables.find(pending.first) != _trackables.end()) {
			pending.second();
		}
	}
	_pending.clear();
}


void
SignalManager2::register_signal(SignalBase* signal)
{
	boost::mutex::scoped_lock lm(_signals_mutex);
	_signals.insert(signal);
}


void
SignalManager2::unregister_signal(SignalBase* signal)
{
	boost::mutex::scoped_lock lm(_signals_mutex);
	_signals.erase(signal);
}


void
SignalManager2::register_trackable(Trackable* trackable)
{
	boost::mutex::scoped_lock lm(_trackables_mutex);
	_trackables.insert(trackable);
}


void SignalManager2::unregister_trackable(Trackable* trackable)
{
	boost::mutex::scoped_lock lm(_trackables_mutex);
	_trackables.erase(trackable);
}


void
SignalManager2::disconnect(SignalBase* signal, int id)
{
	boost::mutex::scoped_lock lm(_signals_mutex);
	if (_signals.find(signal) != _signals.end()) {
		signal->disconnect(id);
	}
}


void
SignalManager2::wake()
{
	process_pending();
}

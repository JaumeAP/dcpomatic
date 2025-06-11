/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "signaller.h"
#include "util.h"
#include <boost/signals2.hpp>
#include <utility>


/** Wrapper for a boost::signals2::signal that asserts that emissions are made from the
 *  UI thead, so anything connected to it will be guaranteed to be called from there.
 */
template <typename T>
class UISignal
{
public:
	template <typename... Args>
	void emit_ui(Signaller* signaller, Args&&... args) const
	{
		signaller->emit(boost::bind(boost::ref(_wrapped), std::forward<Args>(args)...));
	}

	template <typename... Args>
	void emit(Args&&... args) const
	{
		ensure_ui_thread();
		_wrapped(std::forward<Args>(args)...);
	}

	template <typename... Args>
	boost::signals2::connection connect(Args&&... args)
	{
		return _wrapped.connect(std::forward<Args>(args)...);
	}

	template <typename... Args>
	void operator()(Args&&... args) const
	{
		emit(std::forward<Args>(args)...);
	}

private:
	boost::signals2::signal<T> _wrapped;
};


#endif


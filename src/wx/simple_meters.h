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


#ifndef DCPOMATIC_SIMPLE_METERS_H
#define DCPOMATIC_SIMPLE_METERS_H


#include "lib/dcpomatic_time.h"
#include "text_size_cache.h"
#include <memory>
#include <vector>


class Butler;
class LevelCalculator;
class wxPanel;
class wxShowEvent;
class wxWindow;


class SimpleMeters
{
public:
	SimpleMeters(wxWindow* parent);

	SimpleMeters(SimpleMeters const&) = delete;
	SimpleMeters& operator=(SimpleMeters const&) = delete;

	wxWindow* get() const;

	bool update(std::shared_ptr<LevelCalculator> calculator, dcpomatic::DCPTime time);
	void clear();

private:
	void paint();
	void shown(wxShowEvent& ev);

	wxPanel* _panel;
	std::vector<float> _peaks;

	std::weak_ptr<LevelCalculator> _level_calculator;

	TextSizeCache _sizes;
};


#endif


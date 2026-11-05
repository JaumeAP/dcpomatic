/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


#pragma once


#include "dcpomatic_time.h"
#include "types.h"
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>


class DCPAsset
{
public:
	enum class Type {
		VIDEO,
		AUDIO,
		TEXT,
		ATMOS
	};

	/** @param entry_point first frame of the asset to be played, if it is not the first
	 *  @param duration number of frames of the asset to be played, if it is not all that is after the entry point
	 *  @param name_in_dcp name for the file in the DCP's folder, if it is not to be the file's own name
	 */
	DCPAsset(
		Type type,
		boost::filesystem::path file,
		dcpomatic::DCPTimePeriod period,
		Frame entry_point = 0,
		boost::optional<Frame> duration = {},
		boost::optional<boost::filesystem::path> name_in_dcp = {}
		)
		: _type(type)
		, _file(file)
		, _period(period)
		, _entry_point(entry_point)
		, _duration(duration)
		, _name_in_dcp(name_in_dcp)
	{}

	Type type() const {
		return _type;
	}
	boost::filesystem::path file() const {
		return _file;
	}
	dcpomatic::DCPTimePeriod period() const {
		return _period;
	}
	Frame entry_point() const {
		return _entry_point;
	}
	boost::optional<Frame> duration() const {
		return _duration;
	}
	/** @return the name for the file in the DCP's folder: the file's own name unless another was given */
	boost::filesystem::path name_in_dcp() const {
		return _name_in_dcp.get_value_or(_file.filename());
	}
	/** @return the name for the file in the DCP's folder if one was given.  A given name is made from the asset's ID, so the
	 *  same name means the same asset.
	 */
	boost::optional<boost::filesystem::path> name_given() const {
		return _name_in_dcp;
	}

private:
	Type _type;
	boost::filesystem::path _file;
	dcpomatic::DCPTimePeriod _period;
	Frame _entry_point = 0;
	boost::optional<Frame> _duration;
	boost::optional<boost::filesystem::path> _name_in_dcp;
};


/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_REUSABLE_REEL_ASSET_H
#define DCPOMATIC_REUSABLE_REEL_ASSET_H


#include "dcpomatic_time.h"
#include "reuse_type.h"
#include <dcp/reel_asset.h>


class Film;
class Playlist;


class ReusableReelAsset
{
public:
	ReusableReelAsset(std::shared_ptr<dcp::ReelFileAsset> asset_, dcpomatic::DCPTimePeriod period_, ReuseType type_)
		: asset(asset_)
		, period(period_)
		, type(type_)
	{}

	/** The asset */
	std::shared_ptr<dcp::ReelFileAsset> asset;
	/** Period that this asset covers in the DCP */
	dcpomatic::DCPTimePeriod period;
	/** How this asset is re-used - referred to or copied */
	ReuseType type;
};


std::list<ReusableReelAsset> get_reusable_reel_assets(std::shared_ptr<const Film> film, std::shared_ptr<const Playlist> playlist);


#endif

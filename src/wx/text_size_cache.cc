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


#include "text_size_cache.h"
LIBDCP_DISABLE_WARNINGS
#include <wx/graphics.h>
LIBDCP_ENABLE_WARNINGS
#include <wx/wx.h>


dcp::Size
TextSizeCache::get(wxGraphicsContext* gc, wxString const& text)
{
	auto iter = _known.find(text);
	if (iter != _known.end()) {
		return iter->second;
	}

	wxDouble width;
	wxDouble height;
	wxDouble descent;
	wxDouble leading;
	gc->GetTextExtent(text, &width, &height, &descent, &leading);
	auto size = dcp::Size{static_cast<int>(std::round(width)), static_cast<int>(std::round(height))};
	_known[text] = size;

	return size;
}


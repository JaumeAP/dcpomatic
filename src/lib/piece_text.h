/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_PIECE_TEXT_H
#define DCPOMATIC_PIECE_TEXT_H


#include "bitmap_text.h"
#include "dcpomatic_time.h"
#include "rect.h"
#include "types.h"
#include <dcp/subtitle_string.h>
#include <list>


class Image;


class PieceText
{
public:
	explicit PieceText (std::weak_ptr<const Content> content, std::weak_ptr<const TextContent> text, dcpomatic::ContentTime time)
		: _content (content)
		, _text (text)
		, _time (time)
	{}

	std::weak_ptr<const Content> content() const {
		return _content;
	}

	std::weak_ptr<const TextContent> text() const {
		return _text;
	}

	dcpomatic::ContentTime time () const {
		return _time;
	}

private:
	std::weak_ptr<const Content> _content;
	std::weak_ptr<const TextContent> _text;
	dcpomatic::ContentTime _time;
};


class PieceBitmapTextStart : public PieceText
{
public:
	PieceBitmapTextStart (std::weak_ptr<const Content> content, std::weak_ptr<const TextContent> text, dcpomatic::ContentTime time, std::shared_ptr<Image> image, dcpomatic::Rect<double> area)
		: PieceText (content, text, time)
		, sub (image, area)
	{}

	/* Our text, with its rectangle unmodified by any offsets or scales that the content specifies */
	BitmapText sub;
};


/** A text caption.  We store the time period separately (as well as in the dcp::SubtitleStrings)
 *  as the dcp::SubtitleString timings are sometimes quite heavily quantised and this causes problems
 *  when we want to compare the quantised periods to the unquantised ones.
 */
class PieceStringTextStart : public PieceText
{
public:
	PieceStringTextStart (std::weak_ptr<const Content> content, std::weak_ptr<const TextContent> text, dcpomatic::ContentTime time, std::list<dcp::SubtitleString> s)
		: PieceText (content, text, time)
		, subs (s)
	{}

	std::list<dcp::SubtitleString> subs;
};


class PieceTextStop : public PieceText
{
public:
	PieceTextStop (std::weak_ptr<const Content> content, std::weak_ptr<const TextContent> text, dcpomatic::ContentTime time)
		: PieceText (content, text, time)
	{}
};


#endif

/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "lib/butler.h"
#include "lib/closed_text_display.h"
#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcpomatic_time.h"
#include "lib/image.h"
#include "lib/film.h"
#include "lib/player.h"
#include "lib/text_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <memory>


BOOST_AUTO_TEST_CASE(closed_text_display_test1)
{
	auto text = content_factory("test/data/frames.srt");
	auto film = new_test_film2("closed_text_display_test1", text);

	DCPTextTrack track("test", dcp::LanguageTag("de-DE"));
	text[0]->text[0]->set_type(TextType::CLOSED_CAPTION);
	text[0]->text[0]->set_dcp_track(track);
	text[0]->text[0]->set_use(true);

	Player player(film, Image::Alignment::COMPACT);
	auto butler = std::make_shared<Butler>(
		film, player, AudioMapping(), 2, boost::bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::PADDED, true, false, Butler::Audio::DISABLED
		);

	auto await_butler = [butler]() {
		while (butler->video_frames_in_buffer() < 4) {
			dcpomatic_sleep_seconds(1);
		}
	};

	await_butler();

	ClosedTextDisplay display;
	display.set_butler(butler);

	display.update(dcpomatic::DCPTime::from_frames(2, 24), track);
	BOOST_CHECK(display.current() == std::vector<std::string>{ "2" });

	butler->seek({}, true);
	await_butler();
	display.update(dcpomatic::DCPTime::from_frames(0, 24), track);
	BOOST_CHECK(display.current() == std::vector<std::string>{ "0" });

	display.clear();
	BOOST_CHECK(display.current().empty());

	butler->seek(dcpomatic::DCPTime::from_frames(100, 24), true);
	display.update(dcpomatic::DCPTime::from_frames(100, 24), track);
	BOOST_CHECK(display.current().empty());
}


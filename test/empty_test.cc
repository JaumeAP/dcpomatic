/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/empty_test.cc
 *  @brief Test the creation of Empty objects.
 *  @ingroup feature
 */


#include "lib/audio_content.h"
#include "lib/dcp_content_type.h"
#include "lib/decoder.h"
#include "lib/empty_audio.h"
#include "lib/empty_video.h"
#include "lib/film.h"
#include "lib/image_content.h"
#include "lib/player.h"
#include "lib/ratio.h"
#include "lib/video_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::list;
using std::make_shared;
using std::shared_ptr;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


BOOST_AUTO_TEST_CASE (empty_video_test1)
{
	auto film = new_test_film2 ("empty_video_test1");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 *     A A A     B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime::from_frames(2, vfr));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	EmptyVideo black (film, film->playlist(), film->playlist()->length(film));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 2U);
	auto i = black._periods.begin();
	BOOST_CHECK (i->first.from == DCPTime::from_frames(0, vfr));
	BOOST_CHECK (i->first.to ==   DCPTime::from_frames(2, vfr));
	++i;
	BOOST_CHECK (i->first.from == DCPTime::from_frames(5, vfr));
	BOOST_CHECK (i->first.to ==   DCPTime::from_frames(7, vfr));
}


BOOST_AUTO_TEST_CASE (empty_audio_test1)
{
	auto film = new_test_film2 ("empty_audio_test1");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Make this content look like it has audio so we can test the EmptyAudio class */
	contentA->audio = make_shared<AudioContent>(contentA.get());
	contentB->audio = make_shared<AudioContent>(contentB.get());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 *     A A A     B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime::from_frames(2, vfr));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	EmptyAudio silent (film, film->playlist(), film->playlist()->length(film));
	BOOST_REQUIRE_EQUAL (silent._periods.size(), 2U);
	auto i = silent._periods.begin();
	BOOST_CHECK (i->from == DCPTime::from_frames(0, vfr));
	BOOST_CHECK (i->to ==   DCPTime::from_frames(2, vfr));
	++i;
	BOOST_CHECK (i->from == DCPTime::from_frames(5, vfr));
	BOOST_CHECK (i->to ==   DCPTime::from_frames(7, vfr));
}


/** Some tests where the first empty period is not at time 0 */
BOOST_AUTO_TEST_CASE (empty_video_test2)
{
	auto film = new_test_film2 ("empty_video_test2");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 * A A A         B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime(0));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	EmptyVideo black (film, film->playlist(), film->playlist()->length(film));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 1U);
	BOOST_CHECK (black._periods.front().first.from == DCPTime::from_frames(3, vfr));
	BOOST_CHECK (black._periods.front().first.to == DCPTime::from_frames(7, vfr));

	/* position should initially be the start of the first empty period */
	BOOST_CHECK (black.position() == DCPTime::from_frames(3, vfr));

	/* check that done() works */
	BOOST_CHECK (!black.done ());
	black.set_position (DCPTime::from_frames (4, vfr));
	BOOST_CHECK (!black.done ());
	black.set_position (DCPTime::from_frames (7, vfr));
	BOOST_CHECK (black.done ());
}


/** Some tests where the first empty period is not at time 0 */
BOOST_AUTO_TEST_CASE (empty_audio_test2)
{
	auto film = new_test_film2 ("empty_audio_test2");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Make this content look like it has audio so we can test the EmptyAudio class */
	contentA->audio = make_shared<AudioContent>(contentA.get());
	contentB->audio = make_shared<AudioContent>(contentB.get());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 * A A A         B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime(0));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	EmptyAudio silent (film, film->playlist(), film->playlist()->length(film));
	BOOST_REQUIRE_EQUAL (silent._periods.size(), 1U);
	BOOST_CHECK (silent._periods.front().from == DCPTime::from_frames(3, vfr));
	BOOST_CHECK (silent._periods.front().to == DCPTime::from_frames(7, vfr));

	/* position should initially be the start of the first empty period */
	BOOST_CHECK (silent.position() == DCPTime::from_frames(3, vfr));

	/* check that done() works */
	BOOST_CHECK (!silent.done ());
	silent.set_position (DCPTime::from_frames (4, vfr));
	BOOST_CHECK (!silent.done ());
	silent.set_position (DCPTime::from_frames (7, vfr));
	BOOST_CHECK (silent.done ());
}


/** Test for when the film's playlist is not the same as the one passed into Empty */
BOOST_AUTO_TEST_CASE (empty_video_test3)
{
	auto film = new_test_film2 ("empty_video_test3");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 * A A A         B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime(0));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	auto playlist = make_shared<Playlist>();
	playlist->add (film, contentB);
	EmptyVideo black (film, playlist, playlist->length(film));
	BOOST_REQUIRE_EQUAL (black._periods.size(), 1U);
	BOOST_CHECK (black._periods.front().first.from == DCPTime::from_frames(0, vfr));
	BOOST_CHECK (black._periods.front().first.to == DCPTime::from_frames(7, vfr));

	/* position should initially be the start of the first empty period */
	BOOST_CHECK (black.position() == DCPTime::from_frames(0, vfr));
}


/** Test for when the film's playlist is not the same as the one passed into Empty */
BOOST_AUTO_TEST_CASE (empty_audio_test3)
{
	auto film = new_test_film2 ("empty_audio_test3");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Make this content look like it has audio so we can test the EmptyAudio class */
	contentA->audio = make_shared<AudioContent>(contentA.get());
	contentB->audio = make_shared<AudioContent>(contentB.get());

	int const vfr = film->video_frame_rate ();

	/* 0 1 2 3 4 5 6 7
	 * A A A         B
	 */
	contentA->video->set_length (3);
	contentA->set_position (film, DCPTime(0));
	contentB->video->set_length (1);
	contentB->set_position (film, DCPTime::from_frames(7, vfr));

	auto playlist = make_shared<Playlist>();
	playlist->add (film, contentB);
	EmptyAudio silent (film, playlist, playlist->length(film));
	BOOST_REQUIRE_EQUAL (silent._periods.size(), 1U);
	BOOST_CHECK (silent._periods.front().from == DCPTime::from_frames(0, vfr));
	BOOST_CHECK (silent._periods.front().to == DCPTime::from_frames(7, vfr));

	/* position should initially be the start of the first empty period */
	BOOST_CHECK (silent.position() == DCPTime::from_frames(0, vfr));
}


BOOST_AUTO_TEST_CASE (empty_video_test_with_overlapping_content)
{
	auto film = new_test_film2 ("empty_video_test_with_overlapping_content");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	contentA->video->set_length (vfr * 3);
	contentA->set_position (film, DCPTime());
	contentB->video->set_length (vfr * 1);
	contentB->set_position (film, DCPTime::from_seconds(1));

	EmptyVideo black(film, film->playlist(), film->playlist()->length(film));

	BOOST_REQUIRE (black._periods.empty());
}


BOOST_AUTO_TEST_CASE (empty_audio_test_with_overlapping_content)
{
	auto film = new_test_film2 ("empty_audio_test_with_overlapping_content");
	film->set_sequence (false);
	auto contentA = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto contentB = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (contentA);
	film->examine_and_add_content (contentB);
	BOOST_REQUIRE (!wait_for_jobs());

	/* Make this content look like it has audio so we can test the EmptyAudio class */
	contentA->audio = make_shared<AudioContent>(contentA.get());
	contentB->audio = make_shared<AudioContent>(contentB.get());

	int const vfr = film->video_frame_rate ();

	contentA->video->set_length (vfr * 3);
	contentA->set_position (film, DCPTime());
	contentB->video->set_length (vfr * 1);
	contentB->set_position (film, DCPTime::from_seconds(1));

	EmptyAudio silent(film, film->playlist(), film->playlist()->length(film));

	BOOST_REQUIRE (silent._periods.empty());
}


BOOST_AUTO_TEST_CASE (empty_video_test_with_three_d1)
{
	auto film = new_test_film2 ("empty_video_test_with_three_d1");
	film->set_sequence (false);

	auto both1 = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto left = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto right = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto both2 = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (both1);
	film->examine_and_add_content (left);
	film->examine_and_add_content (right);
	film->examine_and_add_content (both2);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	both1->video->set_length (vfr * 1);
	both1->set_position (film, DCPTime(0));

	left->video->set_length (vfr * 2);
	left->video->set_frame_type (VideoFrameType::THREE_D_LEFT);
	left->set_position (film, DCPTime::from_seconds(2));

	right->video->set_length (vfr * 2);
	right->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	right->set_position (film, DCPTime::from_seconds(3));

	both2->video->set_length (vfr * 1);
	both2->set_position (film, DCPTime::from_seconds(6));

	EmptyVideo black(film, film->playlist(), film->playlist()->length(film));

	for (auto i: black._periods) {
		std::cout << to_string(i.first.from) << " " << to_string(i.first.to) << " " << static_cast<int>(i.second) << "\n";
	}
	BOOST_REQUIRE_EQUAL (black._periods.size(), 4U);
	auto i = black._periods.begin();
	BOOST_CHECK (i->first.from == DCPTime::from_seconds(1));
	BOOST_CHECK (i->first.to ==   DCPTime::from_seconds(2));
	BOOST_CHECK (i->second ==     Eyes::BOTH);
	++i;
	BOOST_CHECK (i->first.from == DCPTime::from_seconds(2));
	BOOST_CHECK (i->first.to ==   DCPTime::from_seconds(3));
	BOOST_CHECK (i->second ==     Eyes::RIGHT);
	++i;
	BOOST_CHECK (i->first.from == DCPTime::from_seconds(4));
	BOOST_CHECK (i->first.to ==   DCPTime::from_seconds(5));
	BOOST_CHECK (i->second ==     Eyes::LEFT);
	++i;
	BOOST_CHECK (i->first.from == DCPTime::from_seconds(5));
	BOOST_CHECK (i->first.to ==   DCPTime::from_seconds(6));
	BOOST_CHECK (i->second ==     Eyes::BOTH);
}


BOOST_AUTO_TEST_CASE (empty_video_test_with_three_d2)
{
	auto film = new_test_film2 ("empty_video_test_with_three_d2");
	film->set_sequence (false);

	auto both = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");
	auto right = make_shared<ImageContent>("test/data/simple_testcard_640x480.png");

	film->examine_and_add_content (both);
	film->examine_and_add_content (right);
	BOOST_REQUIRE (!wait_for_jobs());

	int const vfr = film->video_frame_rate ();

	both->video->set_length (vfr * 1);
	both->set_position (film, DCPTime(0));

	right->video->set_length (vfr * 1);
	right->video->set_frame_type (VideoFrameType::THREE_D_RIGHT);
	right->set_position (film, DCPTime::from_seconds(0));

	EmptyVideo black(film, film->playlist(), film->playlist()->length(film));
	BOOST_REQUIRE (black._periods.empty());
}


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


#include "lib/decoder_factory.h"
#include "lib/image_content.h"
#include "lib/piece.h"
#include "lib/piece_video.h"
#include "lib/shuffler.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static void
push (Shuffler& s, shared_ptr<Film> film, shared_ptr<Content> content, shared_ptr<Decoder> decoder, int frame, Eyes eyes)
{
	auto piece = make_shared<Piece>(film, content, decoder, FrameRateChange(24, 24), false);
	PieceVideo cv;
	cv.frame = frame;
	cv.eyes = eyes;
	s.video (piece, cv);
}

list<PieceVideo> pending_cv;

static void
receive (weak_ptr<Piece>, PieceVideo cv)
{
	pending_cv.push_back (cv);
}

static void
check (int frame, Eyes eyes, int line)
{
	BOOST_REQUIRE_MESSAGE (!pending_cv.empty(), "Check at " << line << " failed.");
	BOOST_CHECK_MESSAGE (pending_cv.front().frame == frame, "Check at " << line << " failed.");
	BOOST_CHECK_MESSAGE (pending_cv.front().eyes == eyes, "Check at " << line << " failed.");
	pending_cv.pop_front();
}

/** A perfect sequence */
BOOST_AUTO_TEST_CASE (shuffler_test1)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	auto film = new_test_film2 ("shuffler_test1");
	auto content = make_shared<ImageContent>("test/data/flat_red.png");
	auto decoder = decoder_factory (film, content, true, {});
	BOOST_REQUIRE (decoder);

	for (int i = 0; i < 10; ++i) {
		push (s, film, content, decoder, i, Eyes::LEFT);
		push (s, film, content, decoder, i, Eyes::RIGHT);
		check (i, Eyes::LEFT, __LINE__);
		check (i, Eyes::RIGHT, __LINE__);
	}
}

/** Everything present but some simple shuffling needed */
BOOST_AUTO_TEST_CASE (shuffler_test2)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	auto film = new_test_film2 ("shuffler_test2");
	auto content = make_shared<ImageContent>("test/data/flat_red.png");
	auto decoder = decoder_factory (film, content, true, {});

	for (int i = 0; i < 10; i += 2) {
		push (s, film, content, decoder, i, Eyes::LEFT);
		push (s, film, content, decoder, i + 1, Eyes::LEFT);
		push (s, film, content, decoder, i, Eyes::RIGHT);
		push (s, film, content, decoder, i + 1, Eyes::RIGHT);
		check (i, Eyes::LEFT, __LINE__);
		check (i, Eyes::RIGHT, __LINE__);
		check (i + 1, Eyes::LEFT, __LINE__);
		check (i + 1, Eyes::RIGHT, __LINE__);
	}
}

/** One missing left eye image */
BOOST_AUTO_TEST_CASE (shuffler_test3)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	auto film = new_test_film2 ("shuffler_test3");
	auto content = make_shared<ImageContent>("test/data/flat_red.png");
	auto decoder = decoder_factory (film, content, true, {});

	push (s, film, content, decoder, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);
	push (s, film, content, decoder, 0, Eyes::RIGHT);
	check (0, Eyes::RIGHT, __LINE__);
	push (s, film, content, decoder, 1, Eyes::LEFT);
	check (1, Eyes::LEFT, __LINE__);
	push (s, film, content, decoder, 1, Eyes::RIGHT);
	check (1, Eyes::RIGHT, __LINE__);
	push (s, film, content, decoder, 2, Eyes::RIGHT);
	push (s, film, content, decoder, 3, Eyes::LEFT);
	push (s, film, content, decoder, 3, Eyes::RIGHT);
	push (s, film, content, decoder, 4, Eyes::LEFT);
	push (s, film, content, decoder, 4, Eyes::RIGHT);
	s.flush ();
	check (2, Eyes::RIGHT, __LINE__);
	check (3, Eyes::LEFT, __LINE__);
	check (3, Eyes::RIGHT, __LINE__);
	check (4, Eyes::LEFT, __LINE__);
	check (4, Eyes::RIGHT, __LINE__);
}

/** One missing right eye image */
BOOST_AUTO_TEST_CASE (shuffler_test4)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	auto film = new_test_film2 ("shuffler_test3");
	auto content = make_shared<ImageContent>("test/data/flat_red.png");
	auto decoder = decoder_factory (film, content, true, {});

	push (s, film, content, decoder, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);
	push (s, film, content, decoder, 0, Eyes::RIGHT);
	check (0, Eyes::RIGHT, __LINE__);
	push (s, film, content, decoder, 1, Eyes::LEFT);
	check (1, Eyes::LEFT, __LINE__);
	push (s, film, content, decoder, 1, Eyes::RIGHT);
	check (1, Eyes::RIGHT, __LINE__);
	push (s, film, content, decoder, 2, Eyes::LEFT);
	push (s, film, content, decoder, 3, Eyes::LEFT);
	push (s, film, content, decoder, 3, Eyes::RIGHT);
	push (s, film, content, decoder, 4, Eyes::LEFT);
	push (s, film, content, decoder, 4, Eyes::RIGHT);
	s.flush ();
	check (2, Eyes::LEFT, __LINE__);
	check (3, Eyes::LEFT, __LINE__);
	check (3, Eyes::RIGHT, __LINE__);
	check (4, Eyes::LEFT, __LINE__);
	check (4, Eyes::RIGHT, __LINE__);
}

/** Only one eye */
BOOST_AUTO_TEST_CASE (shuffler_test5)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	auto film = new_test_film2 ("shuffler_test3");
	auto content = make_shared<ImageContent>("test/data/flat_red.png");
	auto decoder = decoder_factory (film, content, true, {});

	/* One left should come out straight away */
	push (s, film, content, decoder, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);

	/* More lefts should be kept in the shuffler in the hope that some rights arrive */
	for (int i = 0; i < s._max_size; ++i) {
		push (s, film, content, decoder, i + 1, Eyes::LEFT);
	}
	BOOST_CHECK (pending_cv.empty ());

	/* If enough lefts come the shuffler should conclude that there's no rights and start
	   giving out the lefts.
	*/
	push (s, film, content, decoder, s._max_size + 1, Eyes::LEFT);
	check (1, Eyes::LEFT, __LINE__);
}

/** One complete frame (L+R) missing.
    Shuffler should carry on, skipping this frame, as the player will cope with it.
*/
BOOST_AUTO_TEST_CASE (shuffler_test6)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	auto film = new_test_film2 ("shuffler_test3");
	auto content = make_shared<ImageContent>("test/data/flat_red.png");
	auto decoder = decoder_factory (film, content, true, {});

	push (s, film, content, decoder, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);
	push (s, film, content, decoder, 0, Eyes::RIGHT);
	check (0, Eyes::RIGHT, __LINE__);

	push (s, film, content, decoder, 2, Eyes::LEFT);
	push (s, film, content, decoder, 2, Eyes::RIGHT);
	check (2, Eyes::LEFT, __LINE__);
	check (2, Eyes::RIGHT, __LINE__);

	push (s, film, content, decoder, 3, Eyes::LEFT);
	check (3, Eyes::LEFT, __LINE__);
	push (s, film, content, decoder, 3, Eyes::RIGHT);
	check (3, Eyes::RIGHT, __LINE__);
}

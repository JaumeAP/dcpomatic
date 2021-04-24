#include "lib/audio_decoder.h"
#include "lib/content_factory.h"
#include "lib/player.h"
#include "lib/resampler_manager.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using std::make_shared;
using std::shared_ptr;
using namespace dcpomatic;


static
shared_ptr<Content>
get_some_content (shared_ptr<Film> film)
{
	shared_ptr<Content> content = content_factory("test/data/white.wav").front();
	film->examine_and_add_content (content);
	BOOST_REQUIRE (!wait_for_jobs());
	return content;
}


BOOST_AUTO_TEST_CASE (resampler_manager_setup_test)
{
	auto film = new_test_film2 ("resampler_manager_setup_test");

	/* One thing */
	auto ac = get_some_content (film);
	ac->set_position(film, DCPTime::from_seconds(5));
	auto player = make_shared<Player>(film, film->playlist());
	shared_ptr<ResamplerManager> rm = player->_resampler_manager;
	BOOST_CHECK_EQUAL (rm->_groups.size(), 1);

	/* Another one after it */
	ac = get_some_content (film);
	ac->set_position (film, DCPTime::from_seconds(10));
	player.reset(new Player(film, film->playlist()));
	rm = player->_resampler_manager;
	BOOST_CHECK_EQUAL (rm->_groups.size(), 1);
	BOOST_CHECK (rm->_groups[0].start == DCPTime::from_seconds(5));
	BOOST_CHECK (rm->_groups[0].end == DCPTime::from_seconds(15));
	BOOST_REQUIRE_EQUAL (rm->_groups[0].decoders.size(), 2);

	/* Another one before it */
	get_some_content (film);
	player.reset(new Player(film, film->playlist()));
	rm = player->_resampler_manager;
	BOOST_CHECK_EQUAL (rm->_groups.size(), 1);
	BOOST_CHECK (rm->_groups[0].start == DCPTime(0));
	BOOST_CHECK (rm->_groups[0].end == DCPTime::from_seconds(15));
	BOOST_REQUIRE_EQUAL (rm->_groups[0].decoders.size(), 3);

	/* One after everything but not directly */
	ac = get_some_content (film);
	ac->set_position (film, DCPTime::from_seconds(20));
	player.reset(new Player(film, film->playlist()));
	rm = player->_resampler_manager;
	BOOST_CHECK_EQUAL (rm->_groups.size(), 2);
	BOOST_CHECK (rm->_groups[0].start == DCPTime(0));
	BOOST_CHECK (rm->_groups[0].end == DCPTime::from_seconds(15));
	BOOST_REQUIRE_EQUAL (rm->_groups[0].decoders.size(), 3);
	BOOST_CHECK (rm->_groups[1].start == DCPTime::from_seconds(20));
	BOOST_CHECK (rm->_groups[1].end == DCPTime::from_seconds(25));
	BOOST_REQUIRE_EQUAL (rm->_groups[1].decoders.size(), 1);

	/* Add one to join the two groups */
	ac = get_some_content (film);
	ac->set_position (film, DCPTime::from_seconds(15));
	player.reset(new Player(film, film->playlist()));
	rm = player->_resampler_manager;
	BOOST_CHECK_EQUAL (rm->_groups.size(), 1);
	BOOST_CHECK (rm->_groups[0].start == DCPTime());
	BOOST_CHECK (rm->_groups[0].end == DCPTime::from_seconds(25));
	BOOST_REQUIRE_EQUAL (rm->_groups[0].decoders.size(), 5);
}


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


#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/ratio.h"
#include "test.h"
#include <boost/test/unit_test.hpp>


using std::make_shared;


BOOST_AUTO_TEST_CASE(reuse_whole_dcp_test)
{
	boost::filesystem::path const source = "test/data/scaling_test_133_185";
	auto dcp = make_shared<DCPContent>(source);
	auto film = new_test_film("reuse_basic_test", { dcp });
	film->set_container(Ratio::from_id("185"));
	film->set_audio_channels(8);
	make_and_verify_dcp(
		film,
		{
			dcp::VerificationNote::Code::INVALID_STANDARD,
		});

	auto const output = film->dir(film->dcp_name());
	check_file(source / "j2c_97b50c93-ad3a-45d7-8ce0-4b875302a704.mxf", output / "j2c_97b50c93-ad3a-45d7-8ce0-4b875302a704.mxf");
	check_file(source / "pcm_156782ba-4cae-4468-b3ea-1126be23b6d6.mxf", output / "pcm_156782ba-4cae-4468-b3ea-1126be23b6d6.mxf");
}


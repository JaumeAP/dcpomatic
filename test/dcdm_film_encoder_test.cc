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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcdm_film_encoder.h"
#include "lib/dpx_writer.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/job_manager.h"
#include "lib/transcode_job.h"
#include "lib/video_content.h"
#include "test.h"
#include <dcp/filesystem.h>
#include <boost/test/unit_test.hpp>
#include <fstream>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


/** Write @param frames qualifying 12-bit DPX frames into @param dir and return the directory */
static
boost::filesystem::path
make_dpx_sequence(boost::filesystem::path dir, int frames)
{
	dcp::filesystem::remove_all(dir);
	dcp::filesystem::create_directories(dir);
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, dcp::Size(64, 48), Image::Alignment::COMPACT);
	for (int i = 0; i < frames; ++i) {
		memset(image->data()[0], 0x10 * (i + 1), 48 * image->stride()[0]);
		char name[64];
		snprintf(name, sizeof(name), "src_%04d.dpx", i);
		write_dpx(dir / name, image);
	}
	return dir;
}


static
shared_ptr<Content>
dpx_content(shared_ptr<Film> film, boost::filesystem::path dir)
{
	auto content = content_factory(dir)[0];
	film->examine_and_add_content({content});
	BOOST_REQUIRE(!wait_for_jobs());
	content->set_video_frame_rate(film, 24);
	return content;
}


static
int
count_files(boost::filesystem::path dir, string extension)
{
	if (!dcp::filesystem::is_directory(dir)) {
		return 0;
	}
	int n = 0;
	for (auto const& i: dcp::filesystem::directory_iterator(dir)) {
		if (i.path().extension() == extension) {
			++n;
		}
	}
	return n;
}


static
void
run_export(shared_ptr<Film> film, boost::filesystem::path output)
{
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	job->set_encoder(make_shared<DCDMFilmEncoder>(film, job, output));
	JobManager::instance()->add(job);
	BOOST_REQUIRE(!wait_for_jobs());
}


static
void
check_audio(boost::filesystem::path dir, vector<string> labels, int video_frames)
{
	for (auto const& label: labels) {
		auto const path = dir / "audio" / (label + ".wav");
		BOOST_REQUIRE_MESSAGE(dcp::filesystem::exists(path), path.string());
		/* 24-bit mono at 48kHz, 2000 samples per frame at 24fps */
		BOOST_CHECK_EQUAL(dcp::filesystem::file_size(path), 44U + video_frames * 2000U * 3U);
	}
	BOOST_CHECK_EQUAL(count_files(dir / "audio", ".wav"), static_cast<int>(labels.size()));
}


vector<string> const five_one = { "L", "R", "C", "LFE", "Ls", "Rs" };


BOOST_AUTO_TEST_CASE(dcdm_encoder_2d_test)
{
	auto film = new_test_film("dcdm_encoder_2d_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_2d_test_src", 6));
	auto const out = boost::filesystem::path("build/test/dcdm_encoder_2d_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	/* Films are at least one second long, so the 6 source frames are padded to 24 */
	auto const frames = film->length().frames_round(24);
	BOOST_REQUIRE_EQUAL(frames, 24);
	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), frames);
	BOOST_CHECK(dcp::filesystem::exists(out / "frame_00000000.dpx"));
	BOOST_CHECK(dcp::filesystem::exists(out / "frame_00000023.dpx"));
	auto const size = film->frame_size();
	BOOST_CHECK_EQUAL(dcp::filesystem::file_size(out / "frame_00000000.dpx"), 2048U + size.width * size.height * 6U);
	check_audio(out, five_one, frames);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_reels_test)
{
	auto film = new_test_film("dcdm_encoder_reels_test");
	auto a = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_reels_test_src_a", 24));
	auto b = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_reels_test_src_b", 30));
	a->set_position(film, dcpomatic::DCPTime());
	b->set_position(film, dcpomatic::DCPTime::from_frames(24, 24));
	film->set_reel_type(ReelType::BY_VIDEO_CONTENT);
	BOOST_REQUIRE_EQUAL(film->reels().size(), 2U);
	BOOST_REQUIRE_EQUAL(film->reels()[0].duration().frames_round(24), 24);
	BOOST_REQUIRE_EQUAL(film->reels()[1].duration().frames_round(24), 30);

	auto const out = boost::filesystem::path("build/test/dcdm_encoder_reels_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), 0);
	BOOST_CHECK_EQUAL(count_files(out / "reel_01", ".dpx"), 24);
	BOOST_CHECK_EQUAL(count_files(out / "reel_02", ".dpx"), 30);
	BOOST_CHECK(dcp::filesystem::exists(out / "reel_02" / "frame_00000000.dpx"));
	BOOST_CHECK(dcp::filesystem::exists(out / "reel_02" / "frame_00000029.dpx"));
	check_audio(out / "reel_01", five_one, 24);
	check_audio(out / "reel_02", five_one, 30);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_3d_test)
{
	auto film = new_test_film("dcdm_encoder_3d_test");
	auto content = dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_3d_test_src", 6));
	content->video->set_frame_type(VideoFrameType::THREE_D_ALTERNATE);
	film->set_three_d(true);
	BOOST_REQUIRE(!wait_for_jobs());

	auto const out = boost::filesystem::path("build/test/dcdm_encoder_3d_test_out");
	dcp::filesystem::remove_all(out);
	run_export(film, out);

	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), 0);
	auto const frames = film->length().frames_round(24);
	BOOST_CHECK_EQUAL(count_files(out / "left", ".dpx"), frames);
	BOOST_CHECK_EQUAL(count_files(out / "right", ".dpx"), frames);
	check_audio(out, five_one, frames);

	dcp::filesystem::remove_all(out);
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_rejects_low_quality_test)
{
	auto film = new_test_film("dcdm_encoder_rejects_low_quality_test", content_factory("test/data/10bit_video_422.mp4"));
	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	try {
		make_shared<DCDMFilmEncoder>(film, job, "build/test/dcdm_encoder_rejects_low_quality_test_out");
		BOOST_CHECK_MESSAGE(false, "no exception thrown");
	} catch (std::runtime_error& e) {
		BOOST_CHECK_MESSAGE(string(e.what()).find("10bit_video_422.mp4") != string::npos, e.what());
	}
}


BOOST_AUTO_TEST_CASE(dcdm_encoder_nonempty_directory_test)
{
	auto film = new_test_film("dcdm_encoder_nonempty_directory_test");
	dpx_content(film, make_dpx_sequence("build/test/dcdm_encoder_nonempty_directory_test_src", 2));

	auto const out = boost::filesystem::path("build/test/dcdm_encoder_nonempty_directory_test_out");
	dcp::filesystem::remove_all(out);
	dcp::filesystem::create_directories(out);
	{
		std::ofstream f((out / "something").string());
		f << "x";
	}

	auto job = make_shared<TranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	auto encoder = make_shared<DCDMFilmEncoder>(film, job, out);
	BOOST_CHECK_THROW(encoder->go(), std::runtime_error);
	BOOST_CHECK_EQUAL(count_files(out, ".dpx"), 0);

	dcp::filesystem::remove_all(out);
}

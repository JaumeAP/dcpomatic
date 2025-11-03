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


#include "analyse_audio_job.h"
#include "audio_content.h"
#include "copy_dcp_details_to_film.h"
#include "dcp_content.h"
#include "dcp_film_encoder.h"
#include "dcp_transcode_job.h"
#include "film.h"
#include "fix_audio_levels_job.h"
#include "playlist.h"
#include <fmt/format.h>
#include <boost/filesystem.hpp>
#include <string>

#include "i18n.h"


using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;


FixAudioLevelsJob::FixAudioLevelsJob(boost::filesystem::path input_dcp_path, boost::filesystem::path output_dcp_path, float leqm_target, bool make_quieter_dcps_louder)
	: Job(shared_ptr<const Film>())
	, _input_dcp_path(input_dcp_path)
	, _output_dcp_path(output_dcp_path)
	, _leqm_target(leqm_target)
	, _make_quieter_dcps_louder(make_quieter_dcps_louder)
{

}


string
FixAudioLevelsJob::name() const
{
	return fmt::format(_("Correcting audio levels of {} to {}"), _input_dcp_path.string(), _leqm_target);
}


string
FixAudioLevelsJob::json_name() const
{
	return N_("fix_audio_levels");
}


void
FixAudioLevelsJob::run()
{
	auto input_dcp = make_shared<DCPContent>(_input_dcp_path);
	auto film = std::make_shared<Film>(_output_dcp_path);
	input_dcp->examine(film, shared_from_this(), true);
	film->add_content({input_dcp});
	copy_dcp_settings_to_film(input_dcp, film);

	auto playlist = make_shared<Playlist>();
	playlist->add(film, input_dcp);

	auto set_progress_wrapper = [this](Job* job) {
		if (auto p = job->progress()) {
			set_progress(*p);
		} else {
			set_progress_unknown();
		}
	};

	AnalyseAudioJob analyse(film, playlist, false, boost::none);
	std::cout << "AAJ is " << &analyse << "\n";
	analyse.Progress.connect(boost::bind<void>(set_progress_wrapper, &analyse));
	try {
		std::cout << "-> calling analyse.run\n";
		analyse.run();
		std::cout << "<- calling analyse.run\n";
	} catch (exception& e) {
		std::cout << "analyse failed.\n";
		set_progress(1);
		set_state(FINISHED_ERROR);
		set_error(e.what());
		return;
	}

	auto const level = analyse.analysis().leqm();
	DCPOMATIC_ASSERT(level);

	if ((!_make_quieter_dcps_louder && *level < _leqm_target) || !input_dcp->audio) {
		std::cout << "nothing to do.\n";
		set_progress(1);
		set_state(FINISHED_OK);
	}

	input_dcp->audio->set_gain(_leqm_target - *level);

	DCPTranscodeJob transcode(film, TranscodeJob::ChangedBehaviour::IGNORE);
	transcode.Progress.connect(boost::bind<void>(set_progress_wrapper, &transcode));
	transcode.set_encoder(make_shared<DCPFilmEncoder>(film, shared_from_this(), true));
	try {
		std::cout << "transcode.\n";
		transcode.run();
	} catch (exception& e) {
		std::cout << "transcode failed " << e.what() << "\n";
		set_progress(1);
		set_state(FINISHED_ERROR);
		set_error(e.what());
		return;
	}

	set_progress(1);
	set_state(FINISHED_OK);
}

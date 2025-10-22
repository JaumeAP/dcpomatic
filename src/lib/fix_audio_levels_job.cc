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

	AnalyseAudioJob analyse(film, playlist, false, boost::none);
	analyse.run();

	auto const level = analyse.analysis().leqm();
	DCPOMATIC_ASSERT(level);

	if ((!_make_quieter_dcps_louder && *level < _leqm_target) || !input_dcp->audio) {
		set_progress(1);
		set_state(FINISHED_OK);
	}

	input_dcp->audio->set_gain(_leqm_target - *level);
	auto transcode = make_shared<DCPTranscodeJob>(film, TranscodeJob::ChangedBehaviour::IGNORE);
	transcode->set_encoder(make_shared<DCPFilmEncoder>(film, transcode, true));
	transcode->run();

	set_progress(1);
	set_state(FINISHED_OK);
}

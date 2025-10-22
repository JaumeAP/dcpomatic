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


#include "job.h"


class FixAudioLevelsJob : public Job
{
public:
	/** @param input_dcp_path Directory containing the DCP to process.
	 *  @param output_dcp_path Directory in which to write the fixed DCP (if required).  The DCP will be created as a directory
	 *  within output_dcp_path.
	 *  @param leqm_target LEQ(m) target.
	 *  @param make_quieter_dcps_louder true to increase the gain of DCPs that are less than leqm_target, false to only correct those which are louder.
	 */
	FixAudioLevelsJob(boost::filesystem::path input_dcp_path, boost::filesystem::path output_dcp_path, float leqm_target, bool make_quieter_dcps_louder);

	std::string name() const override;
	std::string json_name() const override;
	void run() override;

private:
	boost::filesystem::path _input_dcp_path;
	boost::filesystem::path _output_dcp_path;
	float _leqm_target;
	bool _make_quieter_dcps_louder;
};


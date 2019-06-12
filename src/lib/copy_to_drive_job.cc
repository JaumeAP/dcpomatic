/*
    Copyright (C) 2019-2020 Carl Hetherington <cth@carlh.net>

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

#include "dist_writer_messages.h"
#include "copy_to_drive_job.h"
#include "compose.hpp"
#include "exceptions.h"
#include <dcp/raw_convert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::min;
using boost::shared_ptr;
using dcp::raw_convert;

CopyToDriveJob::CopyToDriveJob (boost::filesystem::path dcp, Drive drive, boost::process::opstream& to_writer, boost::process::ipstream& from_writer)
	: Job (shared_ptr<Film>())
	, _dcp (dcp)
	, _drive (drive)
	, _to_writer (to_writer)
	, _from_writer (from_writer)
{

}

string
CopyToDriveJob::name () const
{
	return String::compose (_("Copying %1 to %2"), _dcp.filename().string(), _drive.description());
}

string
CopyToDriveJob::json_name () const
{
	return N_("copy");
}

void
CopyToDriveJob::run ()
{
	_to_writer << _dcp.string() << "\n";
	_to_writer << _drive.internal_name() << "\n";
	_to_writer.flush ();

	while (true) {
		string s;
		getline (_from_writer, s);
		if (s == DIST_WRITER_OK) {
			set_state (FINISHED_OK);
			return;
		} else if (s == DIST_WRITER_ERROR) {
			string m;
			getline (_from_writer, m);
			string n;
			getline (_from_writer, n);
			throw CopyError (m, raw_convert<int>(n));
		} else if (s == DIST_WRITER_PROGRESS) {
			string p;
			getline (_from_writer, p);
			set_progress (raw_convert<float>(p));
		}
	}
}

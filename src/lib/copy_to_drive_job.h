/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "cross.h"
#include "job.h"
#include <boost/process.hpp>

class CopyToDriveJob : public Job
{
public:
	CopyToDriveJob (boost::filesystem::path dcp, Drive drive, boost::process::opstream& to_writer, boost::process::ipstream& from_writer);

	std::string name () const;
	std::string json_name () const;
	void run ();

private:
	void count (boost::filesystem::path dir, uint64_t& total_bytes);
	void copy (boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total);
	boost::filesystem::path _dcp;
	Drive _drive;
	boost::process::opstream& _to_writer;
	boost::process::ipstream& _from_writer;
};

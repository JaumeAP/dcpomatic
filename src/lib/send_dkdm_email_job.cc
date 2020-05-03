/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include "send_dkdm_email_job.h"
#include "compose.hpp"
#include "film.h"
#include "cinema_kdms.h"
#include <list>

#include "i18n.h"

using std::string;
using std::list;
using std::vector;
using boost::shared_ptr;

/** @param kdms KDMs to email.
 *  @param container_name_format Format to ues for folders / ZIP files.
 *  @param filename_format Format to use for filenames.
 *  @param name_values Values to substitute into \p container_name_format and \p filename_format.
 *  @param cpl_name Name of the CPL that the KDMs are for.
 */
SendDKDMEmailJob::SendDKDMEmailJob (
	vector<RecipientWithDKDM> kdms,
	dcp::NameFormat filename_format,
	dcp::NameFormat::Map name_values,
	string cpl_name
	)
	: Job (shared_ptr<Film>())
	, _filename_format (filename_format)
	, _name_values (name_values)
	, _cpl_name (cpl_name)
	, _kdms (kdms)
{

}


SendDKDMEmailJob::~SendDKDMEmailJob ()
{
	stop_thread ();
}


string
SendDKDMEmailJob::name () const
{
	dcp::NameFormat::Map::const_iterator i = _name_values.find ('f');
	if (i == _name_values.end() || i->second.empty()) {
		return _("Email DKDMs");
	}

	return String::compose (_("Email DKDMs for %1"), i->second);
}


string
SendDKDMEmailJob::json_name () const
{
	return N_("send_dkdm_email");
}


void
SendDKDMEmailJob::run ()
{
	set_progress_unknown ();
	RecipientWithDKDM::email (_kdms, _filename_format, _name_values, _cpl_name);
	set_progress (1);
	set_state (FINISHED_OK);
}

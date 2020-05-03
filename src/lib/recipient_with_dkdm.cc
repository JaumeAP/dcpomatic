/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "recipient_with_dkdm.h"
#include "config.h"
#include "dcpomatic_log.h"
#include "emailer.h"
#include "exceptions.h"
#include "zipper.h"
#include "util.h"
#include <boost/foreach.hpp>

#include "i18n.h"

using std::list;
using std::vector;
using std::string;
using boost::shared_ptr;

void
RecipientWithDKDM::make_zip_file (boost::filesystem::path zip_file, dcp::NameFormat name_format, dcp::NameFormat::Map name_values) const
{
	Zipper zipper (zip_file);

	name_values['i'] = kdm.cpl_id ();
	string const name = careful_string_filter(name_format.get(name_values, ".xml"));
	zipper.add (name, kdm.as_xml());

	zipper.close ();
}


/** Email one ZIP file per recipient.
 *  @param kdms KDMs to email.
 *  @param filename_format Format of filenames to use.
 *  @param name_values Values to substitute into \p container_name_format and \p filename_format.
 *  @param cpl_name Name of the CPL that the KDMs are for.
 */
void
RecipientWithDKDM::email (
	vector<RecipientWithDKDM> kdms,
	dcp::NameFormat filename_format,
	dcp::NameFormat::Map name_values,
	string cpl_name
	)
{
	Config* config = Config::instance ();

	if (config->mail_server().empty()) {
		throw NetworkError (_("No mail server configured in preferences"));
	}

	BOOST_FOREACH (RecipientWithDKDM const & i, kdms) {

		if (i.recipient->emails.empty()) {
			continue;
		}

		boost::filesystem::path zip_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
		boost::filesystem::create_directories (zip_file);
		zip_file /= filename_format.get(name_values, ".zip");
		i.make_zip_file (zip_file, filename_format, name_values);

		string subject = config->kdm_subject();
		boost::algorithm::replace_all (subject, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (subject, "$START_TIME", name_values['b']);
		boost::algorithm::replace_all (subject, "$END_TIME", name_values['e']);

		string body = config->kdm_email().c_str();
		boost::algorithm::replace_all (body, "$CPL_NAME", cpl_name);
		boost::algorithm::replace_all (body, "$START_TIME", name_values['b']);
		boost::algorithm::replace_all (body, "$END_TIME", name_values['e']);

		Emailer email (config->kdm_from(), i.recipient->emails, subject, body);

		BOOST_FOREACH (string i, config->kdm_cc()) {
			email.add_cc (i);
		}
		if (!config->kdm_bcc().empty ()) {
			email.add_bcc (config->kdm_bcc ());
		}

		email.add_attachment (zip_file, filename_format.get(name_values, ".zip"), "application/zip");

		try {
			email.send (config->mail_server(), config->mail_port(), config->mail_protocol(), config->mail_user(), config->mail_password());
		} catch (...) {
			boost::filesystem::remove (zip_file);
			dcpomatic_log->log ("Email content follows", LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log (email.email(), LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log ("Email session follows", LogEntry::TYPE_DEBUG_EMAIL);
			dcpomatic_log->log (email.notes(), LogEntry::TYPE_DEBUG_EMAIL);
			throw;
		}

		boost::filesystem::remove (zip_file);

		dcpomatic_log->log ("Email content follows", LogEntry::TYPE_DEBUG_EMAIL);
		dcpomatic_log->log (email.email(), LogEntry::TYPE_DEBUG_EMAIL);
		dcpomatic_log->log ("Email session follows", LogEntry::TYPE_DEBUG_EMAIL);
		dcpomatic_log->log (email.notes(), LogEntry::TYPE_DEBUG_EMAIL);
	}
}


int
RecipientWithDKDM::write_files (
	vector<RecipientWithDKDM> kdms,
	boost::filesystem::path directory,
	dcp::NameFormat name_format,
	dcp::NameFormat::Map name_values,
	boost::function<bool (boost::filesystem::path)> confirm_overwrite
	)
{
	int written = 0;

	if (!boost::filesystem::exists (directory)) {
		boost::filesystem::create_directories (directory);
	}

	/* Write KDMs to the specified directory */
	BOOST_FOREACH (RecipientWithDKDM const& i, kdms) {
		name_values['i'] = i.kdm.cpl_id ();
		boost::filesystem::path out = directory / careful_string_filter(name_format.get(name_values, ".xml"));
		if (!boost::filesystem::exists (out) || confirm_overwrite (out)) {
			i.kdm.as_xml (out);
			++written;
		}
	}

	return written;
}

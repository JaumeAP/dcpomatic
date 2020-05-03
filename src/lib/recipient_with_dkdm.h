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

#ifndef DCPOMATIC_RECIPIENT_WITH_DKDM_H
#define DCPOMATIC_RECIPIENT_WITH_DKDM_H

#include "dkdm_recipient.h"
#include <dcp/encrypted_kdm.h>
#include <dcp/name_format.h>

class RecipientWithDKDM
{
public:
	RecipientWithDKDM (boost::shared_ptr<DKDMRecipient> r, dcp::EncryptedKDM k)
		: recipient (r)
		, kdm (k)
	{}

	void make_zip_file (boost::filesystem::path zip_file, dcp::NameFormat name_format, dcp::NameFormat::Map name_values) const;

	static int write_files (
		std::vector<RecipientWithDKDM> kdms, boost::filesystem::path directory,
		dcp::NameFormat name_format, dcp::NameFormat::Map name_values,
		boost::function<bool (boost::filesystem::path)> confirm_overwrite
		);

	static void email (
		std::vector<RecipientWithDKDM> kdms,
		dcp::NameFormat filename_format,
		dcp::NameFormat::Map name_values,
		std::string cpl_name
		);

	boost::shared_ptr<DKDMRecipient> recipient;
	dcp::EncryptedKDM kdm;
};

#endif

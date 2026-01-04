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


#include "config.h"
#include "download_certificate.h"
#include "internet.h"
#include <dcp/exceptions.h>
#include <fmt/format.h>
#include <string>
#include <utility>

#include "i18n.h"


using std::pair;
using std::string;


static
pair<dcp::Certificate, string>
download_gdc_certificate(string const& serial)
{
	auto const url = fmt::format(
		"ftp://{}:{}@ftp.gdc-tech.com/SHA256/{}.crt.pem",
		Config::instance()->gdc_username().get(),
		Config::instance()->gdc_password().get(),
		serial
		);

	try {
		return { dcp::Certificate(get_from_url(url, true, false).as_string()), url };
	} catch (dcp::MiscError& e) {
		throw CertificateDownloadError(fmt::format(_("Could not read certificate file ({})"), e.what()));
	}
}


static
pair<dcp::Certificate, string>
download_barco_certificate(string const& serial)
{
	string const url = fmt::format(
		"sftp://{}:{}@certificates.barco.com/{}xxx/{}/Barco-ICMP.{}_cert.pem",
		Config::instance()->barco_username().get(),
		Config::instance()->barco_password().get(),
		serial.substr(0, 7),
		serial,
		serial
		);

	try {
		return { dcp::Certificate(get_from_url(url, true, false).as_string()), url };
	} catch (dcp::MiscError& e) {
		throw CertificateDownloadError(fmt::format(_("Could not read certificate file ({})"), e.what()));
	}
}


pair<dcp::Certificate, string>
download_certificate(Manufacturer manufacturer, string const& serial)
{
	switch (manufacturer) {
	case Manufacturer::GDC:
		return download_gdc_certificate(serial);
	case Manufacturer::BARCO:
		return download_barco_certificate(serial);
	}

	return {};
}




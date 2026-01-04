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
#include <dcp/raw_convert.h>
#include <fmt/format.h>
#include <string>
#include <utility>

#include "i18n.h"


using std::pair;
using std::string;
using std::vector;
using dcp::raw_convert;
using boost::optional;


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
		throw CertificateReadError(e.what());
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
		throw CertificateReadError(e.what());
	}
}


class Location
{
public:
	Location(string url_, string file_)
		: url(url_)
		, file(file_)
	{}

	string url;
	string file;
};


static void
try_common(vector<Location>& locations, string prefix, string serial)
{
	auto files = ls_url(fmt::format("{}{}xxx/", prefix, serial.substr(0, 3)));

	auto check = [&locations, prefix, files, serial](string format, string file) {
		auto const zip = fmt::format(format, serial);
		if (find(files.begin(), files.end(), zip) != files.end()) {
			locations.push_back({
				fmt::format("{}{}xxx/{}", prefix, serial.substr(0, 3), zip),
				fmt::format(file, serial)
			});
		}
	};

	check("Dolby-DCP2000-{}.dcicerts.zip", "Dolby-DCP2000-{}.cert.sha256.pem");
	check("Dolby-DCP2000-{}.certs.zip", "Dolby-DCP2000-{}.cert.sha256.pem");
	check("dcp2000-{}.dcicerts.zip", "dcp2000-{}.cert.sha256.pem");
	check("dcp2000-{}.certs.zip", "dcp2000-{}.cert.sha256.pem");
	check("Dolby-IMB-{}.dcicerts.zip", "Dolby-IMB-{}.cert.sha256.pem");
	check("imb-{}.dcicerts.zip", "imb-{}.cert.sha256.pem");
	check("Dolby-IMS1000-{}.dcicerts.zip", "Dolby-IMS1000-{}.cert.sha256.pem");
	check("Dolby-IMS2000-{}.dcicerts.zip", "Dolby-IMS2000-{}.cert.sha256.pem");
	check("cert_Dolby-IMS3000-{}-SMPTE.zip", "cert_Dolby-IMS3000-{}-SMPTE.pem");
	check("ims-{}.dcicerts.zip", "ims-{}.cert.sha256.pem");
}


static void
try_cat862(vector<Location>& locations, string prefix, string serial)
{
	int const serial_int = raw_convert<int>(serial);

	string cat862;
	if (serial_int <= 510999) {
		cat862 = "CAT862_510999_and_lower";
	} else if (serial_int >= 617000) {
		cat862 = "CAT862_617000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		cat862 = fmt::format("CAT862_{}-{}", lower, lower + 999);
	}

	locations.push_back({
		fmt::format("{}{}/cert_Dolby256-CAT862-{}.zip", prefix, cat862, serial_int),
		fmt::format("cert_Dolby256-CAT862-{}.pem.crt", serial_int)
	});
}


static void
try_dsp100(vector<Location>& locations, string prefix, string serial)
{
	int const serial_int = raw_convert<int>(serial);

	string dsp100;
	if (serial_int <= 999) {
		dsp100 = "DSP100_053_thru_999";
	} else if (serial_int >= 3000) {
		dsp100 = "DSP100_3000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		dsp100 = fmt::format("DSP100_{}_thru_{}", lower, lower + 999);
	}

	locations.push_back({
		fmt::format("{}{}/cert_Dolby256-DSP100-{}.zip", prefix, dsp100, serial_int),
		fmt::format("cert_Dolby256-DSP100-{}.pem.crt", serial_int)
	});
}


static void
try_cat745(vector<Location>& locations, string prefix, string serial)
{
	int const serial_int = raw_convert<int>(serial.substr(1));

	string cat745;
	if (serial_int <= 999) {
		cat745 = "CAT745_1_thru_999";
	} else if (serial_int >= 6000) {
		cat745 = "CAT745_6000_and_higher";
	} else {
		int const lower = serial_int - (serial_int % 1000);
		cat745 = fmt::format("CAT745_{}_thru_{}", lower, lower + 999);
	}

	locations.push_back({
		fmt::format("{}{}/cert_Dolby-CAT745-{}.zip", prefix, cat745, serial_int),
		fmt::format("cert_Dolby-CAT745-{}.pem.crt", serial_int)
	});
}


static void
try_cp850(vector<Location>& locations, string prefix, string serial)
{
	int const serial_int = raw_convert<int>(serial.substr(1));

	int const lower = serial_int - (serial_int % 1000);
	locations.push_back({
		fmt::format("{}CP850_CAT1600_F{}-F{}/cert_RMB_SPB_MDE_FMA.Dolby-CP850-F{}.zip", prefix, lower, lower + 999, serial_int),
		fmt::format("cert_RMB_SPB_MDE_FMA.Dolby-CP850-F{}.pem.crt", serial_int)
	});
}


static
pair<dcp::Certificate, string>
download_dolby_certificate(string const& serial, std::function<void ()> yield)
{
	/* Try dcp2000, imb and ims prefixes (see mantis #375) */

	string const prefix = "ftp://ftp.cinema.dolby.com/Certificates/";
	vector<Location> locations;

	bool starts_with_digit = false;
	optional<char> starting_char;

	if (!serial.empty()) {
		if (isdigit(serial[0])) {
			starts_with_digit = true;
		} else {
			starting_char = serial[0];
		}
	}

	vector<string> errors;

	if (starts_with_digit) {
		try_common(locations, prefix, serial);
		yield();
		try_cat862(locations, prefix, serial);
		try_dsp100(locations, prefix, serial);
	} else if (starting_char == 'H') {
		try_cat745(locations, prefix, serial);
	} else if (starting_char == 'F') {
		try_cp850(locations, prefix, serial);
	} else {
		errors.push_back(_("Unrecognised serial number format (does not start with a number, H or F)"));
	}

	for (auto const& location: locations) {
		yield();
		try {
			auto data = get_from_zip_url(location.url, location.file, true, true);
			return { dcp::Certificate(get_from_zip_url(location.url, location.file, true, true).as_string()), location.url };
		} catch (dcp::MiscError& e) {
			throw CertificateReadError(e.what());
		}
	}

	string s;
	for (auto e: errors) {
		s += e + "\n";
	}
	throw CertificateReadError(s);
}


static
pair<dcp::Certificate, string>
download_christie_certificate(string serial)
{
	string const prefix = fmt::format(
		"ftp://{}:{}@certificates.christiedigital.com/Certificates/",
		Config::instance()->christie_username().get(),
		Config::instance()->christie_password().get()
		);

	serial.insert(0, 12 - serial.length(), '0');

	optional<string> all_errors;

	try {
		string const url = fmt::format("{}F-IMB/F-IMB_{}_sha256.pem", prefix, serial);
		return { dcp::CertificateChain(get_from_url(url, true, false).as_string()).leaf(), url };
	} catch (std::exception& e) {
		all_errors = e.what();

		try {
			auto const url = fmt::format("{}IMB-S2/IMB-S2_{}_sha256.pem", prefix, serial);
			return { dcp::CertificateChain(get_from_url(url, true, false).as_string()).leaf(), url };
		} catch (std::exception& e) {
			*all_errors += "\n" + string(e.what());
		}
	}

	/* Could be a mix of download and certificate parsing problems, but hopefully this is good enough */
	throw CertificateReadError(*all_errors);
}


pair<dcp::Certificate, string>
download_qube_certificate(Manufacturer manufacturer, string const& serial)
{
	static string const base = "ftp://certificates.qubecinema.com/";
	auto files = ls_url(fmt::format("{}SMPTE-{}/", base, _type));
	if (files.empty()) {
		throw DownloadError(_("Could not read certificates from Qube server."));
	}

	optional<string> name;
	for (auto i: files) {
		if (boost::algorithm::starts_with(i, fmt::format("{}-{}-", _type, serial))) {
			name = i;
			break;
		}
	}

	if (!name) {
		throw DownloadError(fmt::format(_("Could not find serial number {}"), serial));
	}

	try {
		auto const url = fmt::format("{}SMPTE-{}/{}", base, _type, *name);
		return { dcp::Certificate(get_from_url(url, true, false).as_string()), url };
	} catch (dcp::MiscError& e) {
		throw CertificateReadError(e.what());
	}
}


pair<dcp::Certificate, string>
download_certificate(Manufacturer manufacturer, string const& serial, std::function<void ()> yield)
{
	switch (manufacturer) {
	case Manufacturer::GDC:
		return download_gdc_certificate(serial);
	case Manufacturer::BARCO:
		return download_barco_certificate(serial);
	case Manufacturer::DOLBY:
		return download_dolby_certificate(serial, yield);
	case Manufacturer::CHRISTIE:
		return download_christie_certificate(serial);
	case Manufacturer::QUBE:
		return download_qube_certificate(serial);
	}

	return {};
}


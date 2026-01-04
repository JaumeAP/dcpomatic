/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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


#include "barco_alchemy_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/download_certificate.h"
#include "lib/internet.h"
#include "lib/config.h"
#include <boost/algorithm/string.hpp>


using std::string;
using namespace boost::algorithm;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


BarcoAlchemyCertificatePanel::BarcoAlchemyCertificatePanel(DownloadCertificateDialog* dialog)
	: CredentialsDownloadCertificatePanel(
			dialog,
			boost::bind(&Config::barco_username, Config::instance()),
			boost::bind(&Config::set_barco_username, Config::instance(), _1),
			boost::bind(&Config::unset_barco_username, Config::instance()),
			boost::bind(&Config::barco_password, Config::instance()),
			boost::bind(&Config::set_barco_password, Config::instance(), _1),
			boost::bind(&Config::unset_barco_password, Config::instance())
			)
{

}

bool
BarcoAlchemyCertificatePanel::ready_to_download() const
{
	return CredentialsDownloadCertificatePanel::ready_to_download() && _serial->GetValue().Length() == 10;
}


void
BarcoAlchemyCertificatePanel::do_download()
{
	string serial = wx_to_std(_serial->GetValue());
	trim(serial);
	try {
		std::tie(_certificate, _url) = download_certificate(Manufacturer::BARCO, serial);
		_dialog->message()->SetLabel(_("Certificate downloaded"));
		_dialog->setup_sensitivity();
	} catch (std::exception& e) {
		_dialog->message()->SetLabel({});
		error_dialog(this, std_to_wx(e.what()));
	}
}


wxString
BarcoAlchemyCertificatePanel::name() const
{
	return _("Barco Alchemy");
}

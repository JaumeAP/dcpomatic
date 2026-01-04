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


#include "christie_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/download_certificate.h"
#include "lib/internet.h"
#include "lib/config.h"


using std::string;
using boost::optional;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


ChristieCertificatePanel::ChristieCertificatePanel (DownloadCertificateDialog* dialog)
	: CredentialsDownloadCertificatePanel (
			dialog,
			bind(&Config::christie_username, Config::instance()),
			bind(&Config::set_christie_username, Config::instance(), _1),
			bind(&Config::unset_christie_username, Config::instance()),
			bind(&Config::christie_password, Config::instance()),
			bind(&Config::set_christie_password, Config::instance(), _1),
			bind(&Config::unset_christie_password, Config::instance())
			)

{

}


void
ChristieCertificatePanel::do_download ()
{
	string const prefix = fmt::format(
		"ftp://{}:{}@certificates.christiedigital.com/Certificates/",
		Config::instance()->christie_username().get(),
		Config::instance()->christie_password().get()
		);

	string serial = wx_to_std(_serial->GetValue());
	try {
		std::tie(_certificate, _url) = download_certificate(Manufacturer::CHRISTIE, serial, []() { wxYield(); });
		_dialog->message()->SetLabel(_("Certificate downloaded"));
		_dialog->setup_sensitivity();
	} catch (std::exception& e) {
		_dialog->message()->SetLabel({});
		error_dialog(this, std_to_wx(e.what()));
	}
}


wxString
ChristieCertificatePanel::name () const
{
	return _("Christie");
}

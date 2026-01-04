/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "dolby_doremi_certificate_panel.h"
#include "download_certificate_dialog.h"
#include "wx_util.h"
#include "lib/download_certificate.h"
#include "lib/internet.h"
#include "lib/signal_manager.h"
#include "lib/util.h"
#include <dcp/raw_convert.h>
#include <curl/curl.h>
#include <zip.h>
#include <boost/algorithm/string.hpp>


using std::function;
using std::string;
using std::vector;
using namespace boost::algorithm;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::raw_convert;


DolbyDoremiCertificatePanel::DolbyDoremiCertificatePanel(DownloadCertificateDialog* dialog)
	: DownloadCertificatePanel(dialog)
{

}


void
DolbyDoremiCertificatePanel::do_download()
{
	string serial = wx_to_std(_serial->GetValue());
	trim(serial);

	try {
		std::tie(_certificate, _url) = download_certificate(Manufacturer::DOLBY, serial, []() { wxYield(); });
		_dialog->message()->SetLabel(_("Certificate downloaded"));
		_dialog->setup_sensitivity();
	} catch (std::exception& e) {
		_dialog->message()->SetLabel({});
		error_dialog(this, std_to_wx(e.what()));
	}
}


wxString
DolbyDoremiCertificatePanel::name() const
{
	return _("Dolby / Doremi");
}

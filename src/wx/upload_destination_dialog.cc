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


#include "check_box.h"
#include "dcpomatic_choice.h"
#include "dcpomatic_spin_ctrl.h"
#include "password_entry.h"
#include "upload_destination_dialog.h"
#include "wx_util.h"
#include "lib/upload_destination.h"


using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


UploadDestinationDialog::UploadDestinationDialog(wxWindow* parent)
	: TableDialog(parent, _("Upload destination"), 2, 1, true)
{
	add(_("Name"), true);
	_name = add(new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxSize(480, -1)));
	add(_("Protocol"), true);
	_protocol = add(new Choice(this));
	_passive = add(new CheckBox(this, _("Passive")));
	add_spacer();
	_limit_connections = add(new CheckBox(this, _("Limit connections")));
	_maximum_connections = add(new SpinCtrl(this, DCPOMATIC_SPIN_CTRL_WIDTH), 0, wxALIGN_CENTER_VERTICAL);
	_maximum_connections->SetRange(1, 100);
	add(_("Host"), true);
	_host = add(new wxTextCtrl(this, wxID_ANY));
	add(_("Path"), true);
	_path = add(new wxTextCtrl(this, wxID_ANY));
	add(_("User"), true);
	_user = add(new wxTextCtrl(this, wxID_ANY));
	add(_("Password"), true);
	_password = new PasswordEntry(this);
	add(_password->get_panel());

	_protocol->add_entry(_("SCP"), file_transfer_protocol_to_string(FileTransferProtocol::SCP));
	_protocol->add_entry(_("FTP"), file_transfer_protocol_to_string(FileTransferProtocol::FTP));

	_protocol->bind(&UploadDestinationDialog::protocol_changed, this);
	_limit_connections->bind(&UploadDestinationDialog::limit_connections_changed, this);

	layout();

	_name->SetFocus();

	_protocol->set(0);
	protocol_changed();
	_limit_connections->set(false);
	_maximum_connections->set(5);
	limit_connections_changed();
}


void
UploadDestinationDialog::protocol_changed()
{
	auto const ftp = _protocol->get_data() == string("ftp") || _protocol->get_data() == string("ftps");
	_passive->Enable(ftp);
	_passive->SetValue(ftp);
}


void
UploadDestinationDialog::limit_connections_changed()
{
	_maximum_connections->Enable(_limit_connections->get());
}


void
UploadDestinationDialog::set(UploadDestination const& destination)
{
	checked_set(_name, destination.name);
	checked_set(_protocol, static_cast<int>(destination.protocol));
	checked_set(_passive, destination.passive_ftp);
	checked_set(_limit_connections, static_cast<bool>(destination.maximum_connections));
	if (_limit_connections->get()) {
		checked_set(_maximum_connections, *destination.maximum_connections);
	} else {
		checked_set(_maximum_connections, 5);
	}
	checked_set(_host, destination.host);
	checked_set(_path, destination.path);
	checked_set(_user, destination.user);
	checked_set(_password, destination.password);

	protocol_changed();
	limit_connections_changed();
}


vector<UploadDestination>
UploadDestinationDialog::get () const
{
	auto protocol = FileTransferProtocol::SCP;
	if (auto protocol_name = _protocol->get_data()) {
		protocol = file_transfer_protocol_from_string(*protocol_name);
	}

	optional<int> max;
	if (_limit_connections->get()) {
		max = _maximum_connections->get();
	}

	return {
		UploadDestination(
			wx_to_std(_name->GetValue()),
			protocol,
			_passive->get(),
			max,
			wx_to_std(_host->GetValue()),
			wx_to_std(_path->GetValue()),
			wx_to_std(_user->GetValue()),
			_password->get()
			)
	};
}

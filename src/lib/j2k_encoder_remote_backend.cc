/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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
#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "j2k_encoder_remote_backend.h"
#include "player_video.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/asio.hpp>


#include "i18n.h"


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::unique_ptr;
using std::vector;
using boost::optional;
using dcp::raw_convert;


J2KEncoderRemoteBackend::J2KEncoderRemoteBackend (J2KEncoderRemoteBackend&& other)
	: _server (other._server)
	, _backoff (other._backoff)
{

}


vector<dcp::ArrayData>
J2KEncoderRemoteBackend::encode (vector<DCPVideo> const& all_video)
{
	DCPOMATIC_ASSERT (all_video.size() == 1);
	auto video = all_video.front();

	try {
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver resolver (io_service);
		boost::asio::ip::tcp::resolver::query query (_server.host_name(), raw_convert<string>(ENCODE_FRAME_PORT));
		auto endpoint_iterator = resolver.resolve (query);

		auto socket = make_shared<Socket>(_timeout);

		socket->connect (*endpoint_iterator);

		/* Collect all XML metadata */
		xmlpp::Document doc;
		auto root = doc.create_root_node ("EncodingRequest");
		root->add_child("Version")->add_child_text(raw_convert<string>(SERVER_LINK_VERSION));
		video.add_metadata (root);

		LOG_DEBUG_ENCODE (N_("Sending frame %1 to remote"), video.index());

		{
			Socket::WriteDigestScope ds (socket);

			/* Send XML metadata */
			auto xml = doc.write_to_string ("UTF-8");
			socket->write (xml.length() + 1);
			socket->write (reinterpret_cast<uint8_t const*>(xml.c_str()), xml.bytes() + 1);

			/* Send binary data */
			LOG_TIMING("start-remote-send thread=%1", thread_id());
			video.frame()->write_to_socket(socket);
		}

		/* Read the response (JPEG2000-encoded data); this blocks until the data
		   is ready and sent back.
		*/
		Socket::ReadDigestScope ds (socket);
		LOG_TIMING("start-remote-encode thread=%1", thread_id());
		dcp::ArrayData enc(socket->read_uint32());
		LOG_TIMING("start-remote-receive thread=%1", thread_id());
		socket->read (enc.data(), enc.size());
		LOG_TIMING("finish-remote-receive thread=%1", thread_id());
		if (!ds.check()) {
			throw NetworkError ("Checksums do not match");
		}

		LOG_DEBUG_ENCODE (N_("Finished remotely-encoded frame %1"), video.index());

		_backoff = 0;
		return { enc };

	} catch (std::exception& e) {
		if (_backoff < 60) {
			/* back off more */
			_backoff += 10;
		}
		LOG_ERROR (
			N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
			video.index(), _server.host_name(), e.what(), _backoff
			);
		return {};
	}

}


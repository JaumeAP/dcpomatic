/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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
#include "exceptions.h"
#include "player_video.h"
#include "remote_j2k_frame_encoder.h"
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;
using std::string;
using boost::optional;
using dcp::ArrayData;
using dcp::Data;


optional<dcp::ArrayData>
RemoteJ2KFrameEncoder::encode(DCPVideo const& vf)
{
	optional<dcp::ArrayData> encoded;

	try {
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver resolver (io_service);
		boost::asio::ip::tcp::resolver::query query (_server.host_name(), dcp::raw_convert<string>(ENCODE_FRAME_PORT));
		auto endpoint_iterator = resolver.resolve (query);

		auto socket = make_shared<Socket>(_timeout);

		socket->connect (*endpoint_iterator);

		/* Collect all XML metadata */
		xmlpp::Document doc;
		auto root = doc.create_root_node ("EncodingRequest");
		root->add_child("Version")->add_child_text(dcp::raw_convert<string>(SERVER_LINK_VERSION));
		root->add_child("Index")->add_child_text(dcp::raw_convert<string>(vf.index()));
		root->add_child("FramesPerSecond")->add_child_text(dcp::raw_convert<string>(vf.frames_per_second()));
		root->add_child("J2KBandwidth")->add_child_text(dcp::raw_convert<string>(vf.j2k_bandwidth()));
		root->add_child("Resolution")->add_child_text(dcp::raw_convert<string>(int(vf.resolution())));
		vf.frame()->add_metadata(root);

		LOG_DEBUG_ENCODE(N_("Sending frame %1 to remote"), vf.index());

		{
			Socket::WriteDigestScope ds (socket);

			/* Send XML metadata */
			auto xml = doc.write_to_string ("UTF-8");
			socket->write (xml.length() + 1);
			socket->write (reinterpret_cast<uint8_t const *>(xml.c_str()), xml.bytes() + 1);

			/* Send binary data */
			LOG_TIMING("start-remote-send thread=%1", thread_id());
			vf.frame()->write_to_socket(socket);
		}

		/* Read the response (JPEG2000-encoded data); this blocks until the data
		   is ready and sent back.
		   */
		Socket::ReadDigestScope ds (socket);
		LOG_TIMING("start-remote-encode thread=%1", thread_id());
		encoded = ArrayData(socket->read_uint32());
		LOG_TIMING("start-remote-receive thread=%1", thread_id());
		socket->read (encoded->data(), encoded->size());
		LOG_TIMING("finish-remote-receive thread=%1", thread_id());
		if (!ds.check()) {
			throw NetworkError ("Checksums do not match");
		}

		LOG_DEBUG_ENCODE (N_("Finished remotely-encoded frame %1"), vf.index());

		if (_remote_backoff > 0) {
			LOG_GENERAL ("%1 was lost, but now she is found; removing backoff", _server.host_name());
		}

		/* This job succeeded, so remove any backoff */
		_remote_backoff = 0;

	} catch (std::exception& e) {
		if (_remote_backoff < 60) {
			/* back off more */
			_remote_backoff += 10;
		}
		LOG_ERROR (
			N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
			vf.index(), _server.host_name(), e.what(), _remote_backoff
			);
	}

	return encoded;
}


void
RemoteJ2KFrameEncoder::log_thread_start ()
{
       LOG_TIMING("start-encoder-thread thread=%1 server=%2", thread_id(), _server.host_name());
}


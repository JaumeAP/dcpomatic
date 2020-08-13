#include "config.h"
#include "cross.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "encode_server_description.h"
#include "j2k_encoder_remote_backend.h"
#include "player_video.h"
#include "warnings.h"
#include <dcp/raw_convert.h>
DCPOMATIC_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include "i18n.h"


using std::string;
using boost::optional;
using boost::shared_ptr;
using dcp::Data;
using dcp::raw_convert;


J2KEncoderRemoteBackend::J2KEncoderRemoteBackend (EncodeServerDescription server)
	: _server (server)
	, _backoff (0)
{

}


optional<Data>
J2KEncoderRemoteBackend::encode (shared_ptr<DCPVideo> video)
{
	int const timeout = 30;

	try {
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver resolver (io_service);
		boost::asio::ip::tcp::resolver::query query (_server.host_name(), raw_convert<string>(ENCODE_FRAME_PORT));
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve (query);

		shared_ptr<Socket> socket (new Socket(timeout));

		socket->connect (*endpoint_iterator);

		/* Collect all XML metadata */
		xmlpp::Document doc;
		xmlpp::Element* root = doc.create_root_node ("EncodingRequest");
		root->add_child("Version")->add_child_text(raw_convert<string> (SERVER_LINK_VERSION));
		video->add_metadata (root);

		LOG_DEBUG_ENCODE (N_("Sending frame %1 to remote"), video->index());

		{
			Socket::WriteDigestScope ds (socket);

			/* Send XML metadata */
			string xml = doc.write_to_string ("UTF-8");
			socket->write (xml.length() + 1);
			socket->write ((uint8_t *) xml.c_str(), xml.length() + 1);

			/* Send binary data */
			LOG_TIMING("start-remote-send thread=%1", thread_id());
			video->frame()->write_to_socket(socket);
		}

		/* Read the response (JPEG2000-encoded data); this blocks until the data
		   is ready and sent back.
		*/
		Socket::ReadDigestScope ds (socket);
		LOG_TIMING("start-remote-encode thread=%1", thread_id());
		Data e (socket->read_uint32 ());
		LOG_TIMING("start-remote-receive thread=%1", thread_id());
		socket->read (e.data().get(), e.size());
		LOG_TIMING("finish-remote-receive thread=%1", thread_id());
		if (!ds.check()) {
			throw NetworkError ("Checksums do not match");
		}

		LOG_DEBUG_ENCODE (N_("Finished remotely-encoded frame %1"), video->index());

		if (_backoff > 0) {
			LOG_GENERAL ("%1 was lost, but now she is found; removing backoff", _server.host_name());
		}

		/* This job succeeded, so remove any backoff */
		_backoff = 0;

		return e;

	} catch (std::exception& e) {
		if (_backoff < 60) {
			_backoff += 10;
		}
		LOG_ERROR (
			N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
			video->index(), _server.host_name(), e.what(), _backoff
			);

		boost::this_thread::sleep (boost::posix_time::seconds(_backoff));
		return optional<Data>();
	}
}


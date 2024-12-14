/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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
#include "content_store.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "dcpomatic_socket.h"
#include "http_server.h"
#include "spl.h"
#include "util.h"
#include "variant.h"
#include <dcp/raw_convert.h>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <stdexcept>
#include <utility>


using std::make_pair;
using std::make_shared;
using std::runtime_error;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;


Response Response::ERROR_404 = { 404, "<html><head><title>Error 404</title></head><body><h1>Error 404</h1></body></html>"};


HTTPServer::HTTPServer(int port, int timeout)
	: Server(port, timeout)
{

}



Response::Response(int code)
	: _code(code)
{

}


Response::Response(int code, string payload)
	: _code(code)
	, _payload(payload)
{

}


void
Response::add_header(string key, string value)
{
	_headers.push_back(make_pair(key, value));
}


void
Response::send(shared_ptr<Socket> socket)
{
	socket->write(String::compose("HTTP/1.1 %1 OK\r\n", _code));
	switch (_type) {
	case Type::HTML:
		socket->write("Content-Type: text/html; charset=utf-8\r\n");
		break;
	case Type::JSON:
		socket->write("Content-Type: text/json; charset=utf-8\r\n");
		break;
	}
	socket->write(String::compose("Content-Length: %1\r\n", _payload.length()));
	for (auto const& header: _headers) {
		socket->write(String::compose("%1: %2\r\n", header.first, header.second));
	}
	socket->write("\r\n");
	socket->write(_payload);
}


vector<shared_ptr<SignalSPL>>
get_playlists()
{
	vector<shared_ptr<SignalSPL>> playlists;

	if (auto path = Config::instance()->player_playlist_directory()) {
		try {
			for (auto i: dcp::filesystem::directory_iterator(*path)) {
				auto spl = make_shared<SignalSPL>();
				try {
					spl->read(i, ContentStore::instance());
					playlists.push_back(spl);
				} catch (...) {}
			}
		} catch (...) {}
	}

	return playlists;
}


Response
HTTPServer::get(string const& url)
{
	if (url == "/") {
		return Response(200, String::compose(dcp::file_to_string(resources_path() / "web" / "index.html"), variant::dcpomatic_player()));
	} else if (url == "/api/v1/status") {
		nlohmann::json json;
		{
			boost::mutex::scoped_lock lm(_mutex);
			json["playing"] = _playing;
			json["position"] = seconds_to_hms(_position.seconds());
			json["dcp_name"] = _dcp_name;
		}
		auto response = Response(200, json.dump());
		response.set_type(Response::Type::JSON);
		return response;
	} else if (url == "/api/v1/playlists") {
		nlohmann::json json;
		for (auto spl: get_playlists()) {
			json.push_back(spl->as_json_without_content());
		}
		auto response = Response(200, json.dump());
		response.set_type(Response::Type::JSON);
		return response;
	} else if (boost::algorithm::starts_with(url, "/api/v1/playlist/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 5) {
			return Response::ERROR_404;
		}
		for (auto spl: get_playlists()) {
			if (spl->id() == parts[4]) {
				auto response = Response(200, spl->as_json_with_content().dump());
				response.set_type(Response::Type::JSON);
				return response;
			}
		}
		return Response::ERROR_404;
	} else if (url == "/api/v1/content") {
		nlohmann::json json;
		for (auto i: ContentStore::instance()->all()) {
			json.push_back(SPLEntry(i).as_json());
		}
		auto response = Response(200, json.dump());
		response.set_type(Response::Type::JSON);
		return response;
	} else if (boost::algorithm::starts_with(url, "/api/v1/content/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 5) {
			return Response::ERROR_404;
		}
		auto content = ContentStore::instance()->get(parts[4]);
		if (!content) {
			return Response::ERROR_404;
		}
		auto json = SPLEntry(content).as_json();
		auto response = Response(200, json.dump());
		response.set_type(Response::Type::JSON);
		return response;
	} else {
		LOG_HTTP("404 %1", url);
		return Response::ERROR_404;
	}
}


Response
HTTPServer::post(string const& url, vector<uint8_t> const& body)
{
	if (url == "/api/v1/play") {
		emit(boost::bind(boost::ref(Play)));
		auto response = Response(303);
		response.add_header("Location", "/");
		return response;
	} else if (url == "/api/v1/stop") {
		emit(boost::bind(boost::ref(Stop)));
		auto response = Response(303);
		response.add_header("Location", "/");
		return response;
	} else if (boost::algorithm::starts_with(url, "/api/v1/playlist/")) {
		vector<string> parts;
		boost::algorithm::split(parts, url, boost::is_any_of("/"));
		if (parts.size() != 6 || parts[5] != "insert") {
			return Response::ERROR_404;
		}
		bool found = false;
		for (auto spl: get_playlists()) {
			if (spl->id() == parts[4]) {
				nlohmann::json details = nlohmann::json::parse(body);
				spl->insert(
					SPLEntry(ContentStore::instance()->get(details["digest"])),
					details["before"].is_null() ? optional<string>() : details["before"].get<string>()
				);
				if (auto dir = Config::instance()->player_playlist_directory()) {
					spl->write(*dir / (spl->id() + ".xml"));
				}
				found = true;
			}
		}
		if (!found) {
			return Response::ERROR_404;
		}
		return Response(201);

	} else {
		return Response::ERROR_404;
	}
}


Response
HTTPServer::request(vector<string> const& request, vector<uint8_t> const& body)
{
	vector<string> parts;
	boost::split(parts, request[0], boost::is_any_of(" "));
	if (parts.size() != 3) {
		return Response::ERROR_404;
	}

	try {
		if (parts[0] == "GET") {
			LOG_HTTP("GET %1", parts[1]);
			return get(parts[1]);
		} else if (parts[0] == "POST") {
			LOG_HTTP("POST %1", parts[1]);
			return post(parts[1], body);
		}
	} catch (std::exception& e) {
		LOG_ERROR("Error while handling HTTP request: %1", e.what());
	} catch (...) {
		LOG_ERROR_NC("Unknown exception while handling HTTP request");
	}

	LOG_HTTP("404 %1", parts[0]);
	return Response::ERROR_404;
}


void
HTTPServer::handle(shared_ptr<Socket> socket)
{
	class Reader
	{
	public:
		void read_block(boost::system::error_code const& ec, uint8_t* data, std::size_t size)
		{
			if (ec.value() != boost::system::errc::success) {
				_close = true;
				_error_code = ec;
				return;
			}

			for (std::size_t i = 0; i < size; ++i) {
				switch (_state) {
				case State::HEADER:
					if (_line.length() >= 1024) {
						_close = true;
						return;
					}
					_line += data[i];
					if (_line.length() >= 2 && _line.substr(_line.length() - 2) == "\r\n") {
						/* Got a line */
						if (_line.length() == 2) {
							_state = BODY;
						} else if (_request.size() > 64) {
							_close = true;
							return;
						} else if (_line.size() >= 2) {
							_line = _line.substr(0, _line.length() - 2);
							LOG_HTTP("Receive: %1", _line);
							if (_request.empty()) {
								_method = _line.substr(0, _line.find(' '));
							}
							string lower_case = _line;
							transform(lower_case.begin(), lower_case.end(), lower_case.begin(), ::tolower);
							auto const colon = lower_case.find(':');
							string const key = lower_case.substr(0, colon);
							string value = colon != std::string::npos ? lower_case.substr(colon + 1): "";
							boost::trim(value);
							if (key == "content-length") {
								_content_length = dcp::raw_convert<int>(value);
							}
							_request.push_back(_line);
							_line = "";
						}
					}
					break;
				case State::BODY:
					_body.push_back(data[i]);
					if (_body.size() > 65536) {
						_close = true;
						return;
					}
					break;
				}
			}
		}

		bool completed() const {
			return _state == State::BODY && (_method == "GET" || (_method == "POST" && static_cast<int>(_body.size()) == _content_length));
		}

		bool close() const {
			return _close;
		}

		std::vector<uint8_t> body() const {
			return _body;
		}

		boost::system::error_code error_code() const {
			return _error_code;
		}

		std::string method() const {
			return _method;
		}

		vector<std::string> const& request() const {
			return _request;
		}

	private:
		enum State {
			HEADER,
			BODY
		} _state = HEADER;
		std::string _method;
		std::string _line;
		vector<std::string> _request;
		std::vector<uint8_t> _body;
		int _content_length = 0;
		bool _close = false;
		boost::system::error_code _error_code;
	};

	Reader reader;

	vector<uint8_t> buffer(2048);
	socket->set_deadline_from_now(2);
	socket->socket().async_read_some(
		boost::asio::buffer(buffer.data(), buffer.size()),
		[&reader, &buffer, socket](boost::system::error_code const& ec, std::size_t bytes_transferred) {
			reader.read_block(ec, buffer.data(), bytes_transferred);
		});

	while (!reader.completed() && !reader.close() && socket->is_open()) {
		socket->run();
	}

	if (reader.completed() && !reader.close()) {
		try {
			auto response = request(reader.request(), reader.body());
			response.send(socket);
		} catch (runtime_error& e) {
			LOG_ERROR_NC(e.what());
		}
	}

	/* I think we should keep the socket open if the client requested keep-alive, but some browsers
	 * send keep-alive then don't re-use the connection.  Since we can only accept one request at once,
	 * this blocks until our request read (above) times out.  We probably should accept multiple
	 * requests in parallel, but it's easier for not to use close the socket.
	 */
	socket->close();
}

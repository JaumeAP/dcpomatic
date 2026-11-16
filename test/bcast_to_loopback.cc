/*
    Copyright (C) 2026 Jaume A. Puig

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


/** @file  test/bcast_to_loopback.cc
 *  @brief A macOS library, loaded into the unit tests with DYLD_INSERT_LIBRARIES, that fakes a working UDP broadcast.
 *
 *  On some Macs sending a datagram to 255.255.255.255 fails with EHOSTUNREACH on every interface, so
 *  EncodeServerFinder never finds the server that real_encode_with_server starts.  Here every datagram to
 *  255.255.255.255 goes to 127.0.0.1 instead (same port), and is repeated once a second for
 *  BCAST_REPEAT_SECONDS (default 60): the finder's first hello comes before the test's server listens, and its
 *  next one only 10 seconds later, after the encode has finished.  BCAST_LOG=1 logs each redirect.
 *
 *  Both sendto and sendmsg are interposed, as Boost.Asio sends datagrams with sendmsg.
 */


#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>


static void
repeat(sockaddr_in to, std::string data)
{
	auto const env = getenv("BCAST_REPEAT_SECONDS");
	int const seconds = env ? atoi(env) : 60;
	int const fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return;
	}
	for (int i = 0; i < seconds; ++i) {
		sleep(1);
		sendto(fd, data.data(), data.size(), 0, reinterpret_cast<sockaddr const*>(&to), sizeof(to));
	}
	close(fd);
}


/** Rewrite *to if it is the limited broadcast address.
 *  @return true if it was rewritten.
 */
static bool
redirect(sockaddr_in* to)
{
	if (to->sin_family != AF_INET || to->sin_addr.s_addr != htonl(INADDR_BROADCAST)) {
		return false;
	}

	to->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (getenv("BCAST_LOG")) {
		fprintf(stderr, "BCAST->LO port %d at %ld\n", ntohs(to->sin_port), static_cast<long>(time(nullptr)));
	}
	return true;
}


static void
start_repeat(sockaddr_in const& to, iovec const* iov, int iovcnt)
{
	std::string data;
	for (int i = 0; i < iovcnt; ++i) {
		data.append(static_cast<char const*>(iov[i].iov_base), iov[i].iov_len);
	}
	std::thread(repeat, to, data).detach();
}


static ssize_t
bcast_sendto(int fd, void const* buffer, size_t length, int flags, sockaddr const* to, socklen_t to_length)
{
	if (to && to_length >= sizeof(sockaddr_in)) {
		sockaddr_in lo;
		memcpy(&lo, to, sizeof(lo));
		if (redirect(&lo)) {
			iovec iov = { const_cast<void*>(buffer), length };
			start_repeat(lo, &iov, 1);
			return sendto(fd, buffer, length, flags, reinterpret_cast<sockaddr const*>(&lo), sizeof(lo));
		}
	}
	return sendto(fd, buffer, length, flags, to, to_length);
}


static ssize_t
bcast_sendmsg(int fd, msghdr const* message, int flags)
{
	if (message && message->msg_name && message->msg_namelen >= sizeof(sockaddr_in)) {
		sockaddr_in lo;
		memcpy(&lo, message->msg_name, sizeof(lo));
		if (redirect(&lo)) {
			start_repeat(lo, message->msg_iov, message->msg_iovlen);
			auto copy = *message;
			copy.msg_name = &lo;
			copy.msg_namelen = sizeof(lo);
			return sendmsg(fd, &copy, flags);
		}
	}
	return sendmsg(fd, message, flags);
}


struct Interpose
{
	void const* replacement;
	void const* replacee;
};


__attribute__((used)) static Interpose const interpose[] __attribute__((section("__DATA,__interpose"))) = {
	{ reinterpret_cast<void const*>(&bcast_sendto), reinterpret_cast<void const*>(&sendto) },
	{ reinterpret_cast<void const*>(&bcast_sendmsg), reinterpret_cast<void const*>(&sendmsg) },
};

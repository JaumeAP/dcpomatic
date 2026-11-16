/* Fakes a working UDP broadcast for a process on a Mac where sending to 255.255.255.255 fails
 * with EHOSTUNREACH: every datagram to 255.255.255.255 goes to 127.0.0.1 instead (same port).
 * Each such datagram is also repeated once a second for BCAST_REPEAT_SECONDS (default 60),
 * so a listener that starts just after the first send still receives it.
 *
 * Load with DYLD_INSERT_LIBRARIES=/path/bcast_to_loopback.dylib; BCAST_LOG=1 logs each redirect.
 * Interposes both sendto and sendmsg (Boost.Asio sends datagrams with sendmsg).
 */
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

struct repeat {
	struct sockaddr_in to;
	size_t len;
	char data[];
};

static void *
repeat_thread(void *arg)
{
	struct repeat *r = arg;
	char const *env = getenv("BCAST_REPEAT_SECONDS");
	int seconds = env ? atoi(env) : 60;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	for (int i = 0; fd >= 0 && i < seconds; ++i) {
		sleep(1);
		sendto(fd, r->data, r->len, 0, (const struct sockaddr *) &r->to, sizeof(r->to));
	}
	if (fd >= 0) {
		close(fd);
	}
	free(r);
	return NULL;
}

/* Rewrite *to in place if it is the limited broadcast address; returns 1 if it was rewritten */
static int
redirect(struct sockaddr_in *to)
{
	if (to->sin_family != AF_INET || to->sin_addr.s_addr != htonl(INADDR_BROADCAST)) {
		return 0;
	}
	to->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (getenv("BCAST_LOG")) {
		fprintf(stderr, "BCAST->LO port %d at %ld\n", ntohs(to->sin_port), (long) time(NULL));
	}
	return 1;
}

static void
start_repeat(struct sockaddr_in const *to, struct iovec const *iov, int iovcnt)
{
	size_t len = 0;
	for (int i = 0; i < iovcnt; ++i) {
		len += iov[i].iov_len;
	}
	struct repeat *r = malloc(sizeof(struct repeat) + len);
	if (!r) {
		return;
	}
	r->to = *to;
	r->len = len;
	size_t off = 0;
	for (int i = 0; i < iovcnt; ++i) {
		memcpy(r->data + off, iov[i].iov_base, iov[i].iov_len);
		off += iov[i].iov_len;
	}
	pthread_t t;
	if (pthread_create(&t, NULL, repeat_thread, r) == 0) {
		pthread_detach(t);
	} else {
		free(r);
	}
}

static ssize_t
my_sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	if (to && tolen >= sizeof(struct sockaddr_in)) {
		struct sockaddr_in lo;
		memcpy(&lo, to, sizeof(lo));
		if (redirect(&lo)) {
			struct iovec iov = { (void *) buf, len };
			start_repeat(&lo, &iov, 1);
			return sendto(fd, buf, len, flags, (const struct sockaddr *) &lo, sizeof(lo));
		}
	}
	return sendto(fd, buf, len, flags, to, tolen);
}

static ssize_t
my_sendmsg(int fd, const struct msghdr *msg, int flags)
{
	if (msg && msg->msg_name && msg->msg_namelen >= sizeof(struct sockaddr_in)) {
		struct sockaddr_in lo;
		memcpy(&lo, msg->msg_name, sizeof(lo));
		if (redirect(&lo)) {
			start_repeat(&lo, msg->msg_iov, msg->msg_iovlen);
			struct msghdr copy = *msg;
			copy.msg_name = &lo;
			copy.msg_namelen = sizeof(lo);
			return sendmsg(fd, &copy, flags);
		}
	}
	return sendmsg(fd, msg, flags);
}

__attribute__((used)) static struct { const void *replacement; const void *replacee; } interpose[]
	__attribute__((section("__DATA,__interpose"))) = {
	{ (const void *) my_sendto, (const void *) sendto },
	{ (const void *) my_sendmsg, (const void *) sendmsg },
};

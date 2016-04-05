#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <inttypes.h>


int32_t reads(int32_t fd, void *_buf, int32_t count)
{

	char *buf = (char *) _buf;
	int32_t total = 0, r = 0;

	if (count < 0) return -1;
	while (total < count) {
		r = recv(fd, buf + total, count - total,
			 MSG_NOSIGNAL);
		if (r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		if (r == 0)
			return -1;
		total += r;
	}
	return 0;
}

int32_t sends(int32_t fd, const void *_buf, int32_t count)
{
	const char *buf = (const char *) _buf;
	int32_t total = 0, s = 0;

	if (count < 0) return -1;
	while (total < count) {
		s = send(fd, buf + total, count - total,
			 MSG_NOSIGNAL);
		if (s < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		total += s;
	}
	return 0;
}

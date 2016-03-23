#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>


int reads(int fd, void *_buf, int count)
{

	char *buf = (char *) _buf;
	int total = 0, r = 0;

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

int sends(int fd, const void *_buf, int count)
{
	const char *buf = (const char *) _buf;
	int total = 0, s = 0;

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

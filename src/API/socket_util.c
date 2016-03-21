#include <stdio.h>
#include <stdlib.h>
#include <errno.h>


int readmsg(int fd, char* buf, int count)
{
	
	int total = 0, r = 0;

	if (count < 0) return -1;
	while (total < count) {
		r = recv(fd, &buf[total], count - total);
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

int sendmsg(int fd, const char *buf, int count)
{
	int total = 0, s = 0;

	if (count < 0) return -1;
	while (total < count) {
		s = send(fd, &buf[total], count - total)
		if (s < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		total += s;
	}
}

/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: socket_util.c
* Abstract: This c source file for socket helpers.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#include <socket_util.h>

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
/* REVIEW TODO: The style of the if statement here is not correct */
			if (errno == EINTR) continue;
/* REVIEW TODO: Is it possible to return errno here to indicate the nature of the errors? */
			return -1;
		}
		if (r == 0)
			return -1;
		total += r;
	}
	return 0;
}

/* REVIEW TODO: Same for the function sends */
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

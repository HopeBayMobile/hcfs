/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "socket_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <inttypes.h>

#include "global.h"

/************************************************************************
 * *
 * * Function name: get_hcfs_socket_conn
 * *        Inputs:
 * *       Summary: helper function to connect to hcfs socket.
 * *
 * *  Return value: Socket fd if successful.
 * *		    Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t get_hcfs_socket_conn()
{

	int32_t fd, status;
	struct sockaddr_un addr;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, SOCK_PATH);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
	if (status < 0)
		return -errno;

	return fd;
}

/************************************************************************
 * *
 * * Function name: reads
 * *        Inputs:
 * *       Summary: A wrapper function for socket recv.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t reads(int32_t fd, void *_buf, int32_t count)
{

	char *buf = (char *) _buf;
	int32_t total = 0, r = 0;

	if (count < 0)
		return -EINVAL;

	while (total < count) {
		r = recv(fd, buf + total, count - total,
			 MSG_NOSIGNAL);
		if (r < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		if (r == 0)
			return -EIO;
		total += r;
	}
	return 0;
}

/************************************************************************
 * *
 * * Function name: sends
 * *        Inputs:
 * *       Summary: A wrapper function for socket send.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t sends(int32_t fd, const void *_buf, int32_t count)
{
	const char *buf = (const char *) _buf;
	int32_t total = 0, s = 0;

	if (count < 0)
		return -EINVAL;

	while (total < count) {
		s = send(fd, buf + total, count - total,
			 MSG_NOSIGNAL);
		if (s < 0) {
			if (errno == EINTR)
				continue;
			return -errno;
		}
		total += s;
	}
	return 0;
}

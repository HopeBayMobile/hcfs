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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>
#include <stddef.h>

#define SERVER_PATH "event.notify.mock.server"
#define GOODMSG "goodmsg"
#define BADMSG "badmsg"

void *init_mock_server(void)
{
	int32_t r_size;
	int32_t sock_fd, sock_flag, confirm_flag, sock_len;
	int32_t new_sock_fd;
	int32_t replyok = 1;
	int32_t replyerr = 0;
	char buf[1024];
	struct sockaddr_un addr;
	struct timespec timer;

	addr.sun_family = AF_UNIX;
	addr.sun_path[0] = 0;
	strcpy(&(addr.sun_path[1]), SERVER_PATH);

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	sock_flag = fcntl(sock_fd, F_GETFL, 0);
	sock_flag = sock_flag | O_NONBLOCK;
	fcntl(sock_fd, F_SETFL, sock_flag);

	sock_len = 1 + strlen(SERVER_PATH)
		+ offsetof(struct sockaddr_un, sun_path);
	bind(sock_fd, (struct sockaddr*) &addr, sock_len);
	listen(sock_fd, 1);

	while (1) {
		new_sock_fd = accept(sock_fd, NULL, NULL);
		if (new_sock_fd < 0) {
			nanosleep(&timer, NULL);
			continue;
		}

		r_size = 0;
		while (r_size == 0) {
			r_size = recv(new_sock_fd, &buf[0], 1024, MSG_NOSIGNAL);
		}

		printf("%s\n", buf);
		if (strncmp(buf, BADMSG, strlen(BADMSG)) == 0)
			send(new_sock_fd, &replyerr, sizeof(int32_t), MSG_NOSIGNAL);
		else
			send(new_sock_fd, &replyok, sizeof(int32_t), MSG_NOSIGNAL);

		close(new_sock_fd);
	}
}

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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>

#include "global.h"
#define MAX_FILENAME_LEN 255
#ifdef _ANDROID_ENV_
#define ANDROID_INTERNAL 1
#define ANDROID_EXTERNAL 2
#endif

typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN+1];
	char d_type;
} DIR_ENTRY;

int32_t main(int32_t argc, char **argv)
{
	int32_t fd, status, retcode, code;
	uint32_t cmd_len, reply_len;
	struct sockaddr_un addr;
	char buf[4096];
	struct stat tempstat;
	ino_t this_inode;
	int64_t reserved_size = 123;
	uint32_t num_inodes = 0;
	ino_t inode_list[1000];
	int32_t i;
	char pin_type;

	if (argc < 2) {
		printf("Invalid number of arguments\n");
		exit(-EPERM);
	}
	if (strcasecmp(argv[1], "pin") == 0) {
		code = PIN;
		pin_type = 1;
	} else if (strcasecmp(argv[1], "high-pin") == 0) {
		code = PIN;
		pin_type = 2;
	} else if (strcasecmp(argv[1], "unpin") == 0) {
		code = UNPIN;
	} else {
		code = -1;
	}
	if (code < 0) {
		printf("Unsupported action\n");
		exit(-ENOTSUP);
	}

	for (i = 2; i < argc; i++) {
		if (stat(argv[i], &tempstat) < 0) {
			printf("%s does not exist\n", argv[i]);
			exit(-ENOENT);
		} else {
			this_inode = tempstat.st_ino;
		}
		inode_list[num_inodes] = this_inode;
		num_inodes++;

		printf("%s - inode %" PRIu64 " - num_inode %d\n", argv[i],
		       (uint64_t)this_inode, num_inodes);
	}

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	printf("status is %d, err %s\n", status, strerror(errno));
	switch (code) {
	case PIN:

		cmd_len = sizeof(int64_t) + sizeof(char) + sizeof(uint32_t) +
			num_inodes * sizeof(ino_t);
		memcpy(buf, &reserved_size, sizeof(int64_t)); /* Pre-allocating pinned size (be allowed to be 0) */
		memcpy(buf + sizeof(int64_t), &pin_type, sizeof(char));
		memcpy(buf + sizeof(int64_t) + sizeof(char), &num_inodes, /* # of inodes */
			sizeof(uint32_t));
		memcpy(buf + sizeof(int64_t) + sizeof(char) + sizeof(uint32_t), /* inode array */
			inode_list, sizeof(ino_t) * num_inodes);
		send(fd, &code, sizeof(uint32_t), 0);
		send(fd, &cmd_len, sizeof(uint32_t), 0);
		send(fd, buf, cmd_len, 0);

		recv(fd, &reply_len, sizeof(uint32_t), 0);
		recv(fd, &retcode, sizeof(int32_t), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	case UNPIN:
		cmd_len = sizeof(uint32_t) + num_inodes * sizeof(ino_t);
		memcpy(buf, &num_inodes, sizeof(uint32_t)); /* # of inodes */
		memcpy(buf + sizeof(uint32_t), /* inode array */
			inode_list, sizeof(ino_t) * num_inodes);

		send(fd, &code, sizeof(uint32_t), 0);
		send(fd, &cmd_len, sizeof(uint32_t), 0);
		send(fd, buf, cmd_len, 0);

		recv(fd, &reply_len, sizeof(uint32_t), 0);
		recv(fd, &retcode, sizeof(int32_t), 0);
		if (retcode < 0)
			printf("Command error: Code %d, %s\n",
				-retcode, strerror(-retcode));
		else
			printf("Returned value is %d\n", retcode);
		break;
	default:
		break;
	}
	close(fd);
	return 0;
}

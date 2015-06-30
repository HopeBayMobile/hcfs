/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: api_interface.c
* Abstract: The c source file for Defining API for controlling / monitoring
*
* Revision History
* 2015/6/10 Jiahong created this file, and moved prototype here.
*
**************************************************************************/

#include "api_interface.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "macro.h"
#include "global.h"
#include "params.h"
#include "fuseop.h"

/* TODO: Error handling if the socket path is already occupied and cannot
be deleted */

int init_api_interface(void)
{
	int ret, errcode, count;
	int *val;

	write_log(10, "Starting API interface");
	api_sock.addr.sun_family = AF_UNIX;
	strcpy(api_sock.addr.sun_path, "/dev/shm/hcfs_reporter");
	api_sock.fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (api_sock.fd < 0) {
		errcode = errno;
		write_log(0, "Socket error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	ret = bind(api_sock.fd, (struct sockaddr *) &(api_sock.addr),
		sizeof(struct sockaddr_un));

	if (ret < 0) {
		errcode = errno;
		write_log(0, "Bind error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	ret = listen(api_sock.fd, 16);

	if (ret < 0) {
		errcode = errno;
		write_log(0, "Bind error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	for (count = 0; count < 10; count++) {
		write_log(10, "Starting up thread %d\n", count);
		val = malloc(sizeof(int));
		*val = count;
		pthread_create(&(api_local_thread[count]), NULL,
			(void *)api_module, (void *)val);
		val = NULL;
	}

	return 0;

errcode_handle:
	return errcode;
}

int destroy_api_interface(void)
{
	int ret, errcode;

	UNLINK(api_sock.addr.sun_path);
	return 0;
errcode_handle:
	return errcode;
}

void api_module(void *index)
{
	int fd1, size_msg, msg_len;
	struct timespec timer;
	int retcode;
	
	char buf[512];
	char *largebuf;
	char buf_reused;
	int msg_index;
	unsigned int api_code, arg_len, ret_len;

	timer.tv_sec = 0;
	timer.tv_nsec = 100000000;

	write_log(10, "Startup index %d\n", *((int *)index));

	while (hcfs_system->system_going_down == FALSE) {
		fd1 = accept(api_sock.fd, NULL, NULL);
		if (fd1 < 0) {
			nanosleep(&timer, NULL);  /* Sleep for 0.1 sec */
			continue;
		}
		msg_len = 0;
		msg_index = 0;
		largebuf = NULL;
		buf_reused = FALSE;

		/* Read API code */
		while (TRUE) {
			size_msg = recv(fd1, &buf[msg_len + msg_index], 4, 0);
			if (size_msg <= 0)
				break;
			msg_len += size_msg;
			if (msg_len >= 4)
				break;
		}
		if (msg_len < sizeof(unsigned int)) {
			/* Error reading API code. Return EINVAL. */
			write_log(5, "Invalid API code received\n");
			retcode = EINVAL;
			goto return_message;
		}
		memcpy(&api_code, &buf[msg_index], sizeof(unsigned int));
		msg_index += sizeof(unsigned int);
		msg_len -= sizeof(unsigned int);

		/* Read total length of arguments */
		while (TRUE) {
			size_msg = recv(fd1, &buf[msg_len + msg_index], 4, 0);
			if (size_msg <= 0)
				break;
			msg_len += size_msg;
			if (msg_len >= 4)
				break;
		}
		if (msg_len < sizeof(unsigned int)) {
			/* Error reading API code. Return EINVAL. */
			write_log(5, "Invalid arg length received\n");
			retcode = EINVAL;
			goto return_message;
		}
		memcpy(&arg_len, &buf[msg_index], sizeof(unsigned int));
		msg_index += sizeof(unsigned int);
		msg_len -= sizeof(unsigned int);

		if (arg_len < 500) {
			/* Reuse the preallocated buffer */
			largebuf = &buf[msg_index];
			buf_reused = TRUE;
		} else {
			/* Allocate a buffer that's large enough */
			largebuf = malloc(arg_len+20);
			/* If error in allocating, return ENOMEM */
			if (largebuf == NULL) {
				write_log(0, "Out of memory in %s\n", __func__);
				retcode = ENOMEM;
				goto return_message;
			}
			/* If msg_len > 0, copy the rest of the message */
			if (msg_len > 0)
				memcpy(largebuf, &buf[msg_index], msg_len);
		}

		msg_index = 0;
		while (TRUE) {
			size_msg = recv(fd1, &largebuf[msg_len + msg_index],
					1024, 0);
			if (size_msg <= 0)
				break;
			msg_len += size_msg;
			if (msg_len >= arg_len)
				break;
		}
		if (msg_len < arg_len) {
			/* Error reading arguments. Return EINVAL. */
			write_log(5, "Error when reading API arguments\n");
			retcode = EINVAL;
			goto return_message;
		}

		switch (api_code) {
		case TERMINATE:
			pthread_kill(HCFS_mount, SIGHUP);
			hcfs_system->system_going_down = TRUE;
			retcode = 0;
			ret_len = sizeof(int);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &retcode, sizeof(int), 0);
			break;
		case VOLSTAT:
			buf[0] = 0;
			retcode = 0;
			sem_wait(&(hcfs_system->access_sem));
			sprintf(buf, "%lld %lld %lld",
				hcfs_system->systemdata.system_size,
				hcfs_system->systemdata.cache_size,
				hcfs_system->systemdata.cache_blocks);
			sem_post(&(hcfs_system->access_sem));
			write_log(10, "debug stat hcfs %s\n", buf);
			ret_len = strlen(buf)+1;
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, buf, strlen(buf)+1, 0);
			break;
		case TEST:
			retcode = *((int *)index);
			write_log(10, "Index is %d\n", retcode);
			ret_len = sizeof(int);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &retcode, sizeof(int), 0);
			break;
		default:
			retcode = ENOTSUP;
			break;
		}
return_message:
		if (retcode != 0) {
			ret_len = sizeof(int);
			send(fd1, &ret_len, sizeof(unsigned int), 0);
			send(fd1, &retcode, sizeof(int), 0);
		}
		if ((largebuf != NULL) && (buf_reused == FALSE))
			free(largebuf);
		largebuf = NULL;
		buf_reused = FALSE;
		close(fd1);
	}
}


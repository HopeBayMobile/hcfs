/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: fuseproc_comm.c
* Abstract: Code corresponding to communicate with to-cloud process.
*
* Revision History
* 2016/4/18 Kewei created this file.
*
**************************************************************************/

#ifndef GW20_FUSEPROC_COMM_H_
#define GW20_FUSEPROC_COMM_H_
#include "fuseproc_comm.h"

#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <time.h>

/************************************************************************
*
* Function name: communication_contact_window
*        Inputs: void *data
*
*       Summary: This thread function aims to be a contact window between
*                fuse process and other processes.
*
*       Response: Send integer 0 when succeeding in tagging inode. Otherwise
*                 Send -1 on error.
*
*	return : None
*************************************************************************/
void *fuse_communication_contact_window(void *data)
{
	int errcode;
	UPLOADING_COMMUNICATION_DATA uploading_data;
	int communicate_result;
	struct timespec timer;
	int ret;
	int ac_fd;
	const int socket_fd = *(int *)data;

	timer.tv_sec = 0;
	timer.tv_nsec = 100000000;

	/* Wait for some connections */
	while (hcfs_system->system_going_down == FALSE) {
		ac_fd = accept(socket_fd, NULL, NULL);
		if (ac_fd < 0) {
			errcode = errno;
			if (errcode == EAGAIN) {
				nanosleep(&timer, NULL);
				continue;
			} else {
				write_log(0, "Accept socket error in %s."
					" Code %d\n", __func__, errcode);
				nanosleep(&timer, NULL);
				continue;
			}
		}

		recv(ac_fd, &uploading_data,
			sizeof(UPLOADING_COMMUNICATION_DATA), 0);

		ret = fuseproc_set_uploading_info(&uploading_data);
		if (ret < 0) {
			communicate_result = ret;
			write_log(2, "Fail to tag inode %"PRIu64" in %s",
				(uint64_t)uploading_data.inode, __func__);
		} else {
			communicate_result = 0;
			if (uploading_data.is_uploading)
				write_log(10, "Debug: Succeed in tagging inode "
					"%"PRIu64" as UPLOADING\n",
					(uint64_t)uploading_data.inode);
			else
				write_log(10, "Debug: Succeed in tagging inode "
					"%"PRIu64" as NOT_UPLOADING\n",
					(uint64_t)uploading_data.inode);
		}

		send(ac_fd, &communicate_result, sizeof(int), 0);
		close(ac_fd);
	}

	write_log(10, "Debug: Terminate fuse communication contact\n");

	return NULL;
}

/**
 * Initialize communication socket of fuse process.
 *
 * @param communicate_tid Communication thread id array.
 * @param socket_fd File descriptor of listened socket.
 *
 * @return 0 on success init, otherwise negative error code.
 */
int init_fuse_proc_communication(pthread_t *communicate_tid, int *socket_fd)
{
	int ret, i;
	int errcode;
	int socket_flag;
	struct sockaddr_un sock_addr;

	if (!access(FUSE_SOCK_PATH, F_OK))
		UNLINK(FUSE_SOCK_PATH);

	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, FUSE_SOCK_PATH);
	*socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

	ret = bind(*socket_fd, (struct sockaddr *)&sock_addr,
		sizeof(struct sockaddr_un));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Bind socket error in %s. Code %d.\n",
			__func__, errcode);
		return ret;
	}

	socket_flag = fcntl(*socket_fd, F_GETFL, 0);
	socket_flag |= O_NONBLOCK;
	fcntl(*socket_fd, F_SETFL, socket_flag);

	ret = listen(*socket_fd, MAX_FUSE_COMMUNICATION_THREAD);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Listen socket error in %s. Code %d.\n",
			__func__, errcode);
		return ret;
	}

	for (i = 0; i< MAX_FUSE_COMMUNICATION_THREAD ; i++) {
		ret = pthread_create(&communicate_tid[i], NULL,
			fuse_communication_contact_window, socket_fd);
		if (ret < 0) {
			errcode = ret;
			write_log(0, "Creating thread error in %s. Code %d.\n",
					__func__, errcode);
			return -errcode;
		}
	}

	write_log(10, "Communication contact for fuse has been created.\n");
	return 0;

errcode_handle:
	return errcode;
}

int destroy_fuse_proc_communication(pthread_t *communicate_tid, int socket_fd)
{
	int ret, errcode;
	int i;

	for (i = 0; i< MAX_FUSE_COMMUNICATION_THREAD ; i++) {
		pthread_join(*communicate_tid, NULL);
	}

	close(socket_fd);
	UNLINK(FUSE_SOCK_PATH);
	write_log(10, "Debug: destroy fuse communication sockpath\n");
	return 0;

errcode_handle:
	return errcode;
}
#endif

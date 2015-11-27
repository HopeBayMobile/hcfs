#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <libgen.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "socket_serv.h"

#include "global.h"
#include "pin_ops.h"
#include "hcfs_stat.h"
#include "hcfs_sys.h"

SOCK_THREAD thread_pool[MAX_THREAD];

int _get_unused_thread()
{

	int idx;

	for (idx = 0; idx < MAX_THREAD; idx++) {
		if (!thread_pool[idx].in_used) {
			thread_pool[idx].in_used = TRUE;
			return idx;
		}
	}

	return -1;
}

int process_request(int thread_idx)
{

	int fd, ret_code, to_recv, buf_idx;
	int cloud_stat;
	unsigned int api_code, arg_len, ret_len;
	long long cloud_usage;
	long long cache_total, cache_used, cache_dirty;
	long long pin_max, pin_total;
	long long xfer_up, xfer_down;
	long long num_local, num_cloud, num_hybrid;
	char buf_reused;
	char buf[512], res_buf[512];
	char *largebuf, *value;
	ssize_t size_msg, msg_len;

	msg_len = 0;
	ret_len = 0;

	fd = thread_pool[thread_idx].fd;

	size_msg = recv(fd, &buf[0], sizeof(unsigned int), 0);
	if (size_msg < sizeof(unsigned int)) {
		printf("Error recv 1\n");
		return -1;
	}
	memcpy(&api_code, &buf[0], sizeof(unsigned int));
	printf("API code is %d\n", api_code);

	size_msg = recv(fd, &buf[0], sizeof(unsigned int), 0);
	if (size_msg < sizeof(unsigned int)) {
		printf("Error recv 2\n");
		return -1;
	}

	memcpy(&arg_len, &buf[0], sizeof(unsigned int));
	printf("arg_len == %d\n", arg_len);

	if (arg_len < 500) {
		largebuf = &buf[0];
		buf_reused = TRUE;
	} else {
		largebuf = malloc(arg_len+20);
		if (largebuf == NULL)
			return -1;
	}

	while (1) {
		if (arg_len == 0)
			break;

		if ((arg_len - msg_len) > 1024)
			to_recv = 1024;
		else
			to_recv = (arg_len - msg_len);
		size_msg = recv(fd, &largebuf[msg_len], to_recv, 0);
		if (size_msg < 0)
			break;

		msg_len += size_msg;
		if (msg_len >= arg_len)
			break;
	}

	if (msg_len < arg_len)
		return -1;

	switch (api_code) {
	case PIN:
		printf("Pin by path\n");
		ret_code = pin_by_path(largebuf, arg_len);
		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &ret_code, sizeof(int), 0);
		break;

	case UNPIN:
		printf("Unpin by path\n");
		ret_code = unpin_by_path(largebuf, arg_len);
		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &ret_code, sizeof(int), 0);
		break;

	case CHECKDIRSTAT:
		printf("Check dir stat\n");
		ret_len = 0;
		ret_code = check_dir_status(largebuf, arg_len,
					       &num_local, &num_cloud,
					       &num_hybrid);

		if (ret_code < 0) {
			size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
			size_msg = send(fd, &ret_code, sizeof(int), 0);
		}

		memcpy(&(res_buf[ret_len]), &num_local, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &num_cloud, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &num_hybrid, sizeof(long long));
		ret_len += sizeof(long long);

		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, res_buf, ret_len, 0);

		break;

	case CHECKLOC:
		printf("Check file loc\n");
		ret_code = check_file_loc(largebuf, arg_len);
		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &ret_code, sizeof(int), 0);
		break;

	case CHECKPIN:
		printf("Check pin status\n");
		ret_code = check_pin_status(largebuf, arg_len);
		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &ret_code, sizeof(int), 0);
		break;

	case SETCONFIG:
		printf("Set config\n");
		ret_code = set_hcfs_config(largebuf, arg_len);
		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &ret_code, sizeof(int), 0);
		break;

	case GETCONFIG:
		printf("Get config\n");
		ret_code = get_hcfs_config(largebuf, arg_len, &value);
		if (ret_code < 0) {
			ret_len = 0;
			size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
			size_msg = send(fd, &ret_code, sizeof(int), 0);
		} else {
			ret_len = strlen(value);
			memcpy(res_buf, value, ret_len);
			free(value);
			size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
			size_msg = send(fd, res_buf, ret_len, 0);
		}
		break;

	case GETSTAT:
		printf("Get statistics\n");
		ret_len = 0;
		ret_code = get_hcfs_stat(&cloud_usage, &cache_total,
					 &cache_used, &cache_dirty,
					 &pin_max, &pin_total,
					 &xfer_up, &xfer_down,
					 &cloud_stat);
		if (ret_code < 0) {
			size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
			size_msg = send(fd, &ret_code, sizeof(int), 0);
		}

		memcpy(&(res_buf[ret_len]), &cloud_usage, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &cache_total, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &cache_used, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &cache_dirty, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &pin_max, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &pin_total, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &xfer_up, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &xfer_down, sizeof(long long));
		ret_len += sizeof(long long);

		memcpy(&(res_buf[ret_len]), &cloud_stat, sizeof(int));
		ret_len += sizeof(int);

		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, res_buf, ret_len, 0);

		break;

	case RESETXFERSTAT:
		printf("Reset xfer\n");
		ret_len = 0;
		ret_code = reset_xfer_usage();
		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &ret_code, sizeof(int), 0);

		break;

	case SYSREBOOT:
		printf("Reboot\n");
		ret_len = 0;
		ret_code = 0;
		size_msg = send(fd, &ret_len, sizeof(unsigned int), 0);
		size_msg = send(fd, &ret_code, sizeof(int), 0);

		// Force to call reboot
		system("reboot");

		break;
	}

	printf("Get API code - %d from fd - %d\n", api_code, fd);

	close(fd);

	if (largebuf != NULL && !buf_reused)
		free(largebuf);

	thread_pool[thread_idx].in_used = FALSE;
}

int init_server()
{

	int sock_fd, sock_flag;
	int new_sock_fd, thread_idx, ret_code, count;
	char sock_path[200];
	char *sock_dir;
	struct sockaddr_un addr;
	struct timespec timer;


	for (count = 0; count < MAX_THREAD; count++) {
		thread_pool[count].fd = 0;
		thread_pool[count].in_used = FALSE;
	}

	strcpy(sock_path, API_SOCK_PATH);

	/* To wait api socket path ready */
	sock_dir = dirname(sock_path);
	while (1) {
		if (access(sock_dir, F_OK | W_OK) == 0)
			break;
		else
			sleep(1);
	}

	if (access(API_SOCK_PATH, F_OK) == 0)
		unlink(API_SOCK_PATH);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, API_SOCK_PATH);

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0)
		return -1;

	if (bind(sock_fd, (struct sockaddr*) &addr,
		 sizeof(struct sockaddr_un)) < 0)
		return -1;

	sock_flag = fcntl(sock_fd, F_GETFL, 0);
	sock_flag = sock_flag | O_NONBLOCK;
	fcntl(sock_fd, F_SETFL, sock_flag);

	ret_code = listen(sock_fd, 16);
	if (ret_code < 0)
		return -1;

	timer.tv_sec = 0;
	timer.tv_nsec = 100000000;

	while(1) {
		new_sock_fd = accept(sock_fd, NULL, NULL);
		if (new_sock_fd < 0) {
			nanosleep(&timer, NULL);
			continue;
		}

		while (1) {
			thread_idx = _get_unused_thread();
			if (thread_idx < 0) {
				nanosleep(&timer, NULL);
				continue;
			} else
				break;
		}

		thread_pool[thread_idx].fd = new_sock_fd;
		pthread_create(&(thread_pool[thread_idx].thread), NULL,
			       (void *)process_request, (void *)thread_idx);

	}

	for (count = 0; count > MAX_THREAD; count++) {
		if (thread_pool[count].in_used)
			pthread_join(thread_pool[count].thread, NULL);
	}

	close(sock_fd);
	return 0;
}

int main()
{
	init_server();
	return 0;
}

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
#include <inttypes.h>

#include "socket_serv.h"

#include "socket_util.h"
#include "global.h"
#include "pin_ops.h"
#include "hcfs_stat.h"
#include "hcfs_sys.h"
#include "marco.h"

SOCK_THREAD thread_pool[MAX_THREAD];

int do_pin_by_path(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Pin by path\n");
	ret_code = pin_by_path(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_unpin_by_path(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Unpin by path\n");
	ret_code = unpin_by_path(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_check_dir_status(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;
	long long num_local, num_cloud, num_hybrid;

	printf("Check dir stat\n");
	ret_code = check_dir_status(largebuf, arg_len,
				    &num_local, &num_cloud,
				    &num_hybrid);

	printf("%d\n", ret_code);
	if (ret_code < 0) {
		CONCAT_REPLY(&ret_len, sizeof(unsigned int));
		CONCAT_REPLY(&ret_code, sizeof(int));
	} else {
		CONCAT_REPLY(res_size, sizeof(unsigned int));
		CONCAT_REPLY(&num_local, sizeof(long long));
		CONCAT_REPLY(&num_cloud, sizeof(long long));
		CONCAT_REPLY(&num_hybrid, sizeof(long long));
	}

	return ret_code;
}

int do_check_file_loc(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Check file location\n");
	ret_code = check_file_loc(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_check_pin_status(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Check pin status\n");
	ret_code = check_pin_status(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_set_hcfs_config(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Set config\n");
	ret_code = set_hcfs_config(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_get_hcfs_config(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;
	char *value;

	printf("Get config\n");
	ret_code = get_hcfs_config(largebuf, arg_len, &value);

	if (ret_code == 0) {
		ret_len = strlen(value);
		CONCAT_REPLY(&ret_len, sizeof(unsigned int));
		CONCAT_REPLY(value, ret_len);
		free(value);
	} else {
		CONCAT_REPLY(&ret_len, sizeof(unsigned int));
		CONCAT_REPLY(&ret_code, sizeof(int));
	}

	return ret_code;
}

int do_get_hcfs_stat(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;
	int cloud_stat;
	long long vol_usage, cloud_usage;
	long long cache_total, cache_used, cache_dirty;
	long long pin_max, pin_total;
	long long xfer_up, xfer_down;

	printf("Get statistics\n");
	ret_code = get_hcfs_stat(&vol_usage, &cloud_usage, &cache_total,
				 &cache_used, &cache_dirty,
				 &pin_max, &pin_total,
				 &xfer_up, &xfer_down,
				 &cloud_stat);

	if (ret_code < 0) {
		CONCAT_REPLY(&ret_len, sizeof(unsigned int));
		CONCAT_REPLY(&ret_code, sizeof(int));
	} else {
		/* Total size for reply msgs */
		ret_len = sizeof(long long) * 9 + sizeof(int);

		CONCAT_REPLY(&ret_len, sizeof(unsigned int));
		CONCAT_REPLY(&vol_usage, sizeof(long long));
		CONCAT_REPLY(&cloud_usage, sizeof(long long));
		CONCAT_REPLY(&cache_total, sizeof(long long));
		CONCAT_REPLY(&cache_used, sizeof(long long));
		CONCAT_REPLY(&cache_dirty, sizeof(long long));
		CONCAT_REPLY(&pin_max, sizeof(long long));
		CONCAT_REPLY(&pin_total, sizeof(long long));
		CONCAT_REPLY(&xfer_up, sizeof(long long));
		CONCAT_REPLY(&xfer_down, sizeof(long long));
		CONCAT_REPLY(&cloud_stat, sizeof(int));
	}

	return ret_code;
}

int do_reset_xfer_usage(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Reset xfer\n");
	ret_code = reset_xfer_usage(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_toggle_cloud_sync(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Toggle sync\n");
	ret_code = toggle_cloud_sync(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_get_sync_status(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Get sync status\n");
	ret_code = get_sync_status(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int do_reload_hcfs_config(char *largebuf, int arg_len, char *resbuf, int *res_size)
{
	int ret_code;
	int ret_len = 0;

	printf("Reload config\n");
	ret_code = reload_hcfs_config(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(unsigned int));
	CONCAT_REPLY(&ret_code, sizeof(int));

	return ret_code;
}

int32_t _get_unused_thread()
{

	int32_t idx;

	for (idx = 0; idx < MAX_THREAD; idx++) {
		if (!thread_pool[idx].in_used) {
			thread_pool[idx].in_used = TRUE;
			return idx;
		}
	}

	return -1;
}

int32_t process_request(int32_t thread_idx)
{

	int fd, ret_code, res_size;
	unsigned int api_code, arg_len;
	char buf_reused;
	char buf[512], resbuf[512];
	char *largebuf;

	fd = thread_pool[thread_idx].fd;

	if (reads(fd, &api_code, sizeof(unsigned int))) {
		printf("Failed to recv API code");
		goto error;
	}
	printf("API code is %d\n", api_code);

	if (reads(fd, &arg_len, sizeof(unsigned int))) {
		printf("Failed to recv arg length\n");
		goto error;
	}
	printf("Argument length is %d\n", arg_len);

	if (arg_len < 500) {
		largebuf = buf;
		buf_reused = TRUE;
	} else {
		largebuf = malloc(arg_len + 20);
		if (largebuf == NULL)
			goto error;
	}

	if (reads(fd, largebuf, arg_len)) {
		printf("Failed to recv args\n");
		goto error;
	}

	struct cmd {
		unsigned int name;
		int (*cmd_fn)(char *largebuf, int arg_len, char *resbuf, int *res_size);
	};

	struct cmd cmds[] = {
		{PIN,		do_pin_by_path},
		{UNPIN,		do_unpin_by_path},
		{CHECKDIRSTAT,	do_check_dir_status},
		{CHECKLOC,	do_check_file_loc},
		{CHECKPIN,	do_check_pin_status},
		{SETCONFIG,	do_set_hcfs_config},
		{GETCONFIG,	do_get_hcfs_config},
		{GETSTAT,	do_get_hcfs_stat},
		{RESETXFERSTAT,	do_reset_xfer_usage},
		{SETSYNCSWITCH,	do_toggle_cloud_sync},
		{GETSYNCSWITCH,	do_get_sync_status},
	};

	unsigned int n;
	for (n = 0; n < sizeof(cmds) / sizeof(cmds[0]); n++) {
		if (api_code == cmds[n].name) {
			res_size = 0;
			ret_code = cmds[n].cmd_fn(largebuf, arg_len, resbuf, &res_size);
			sends(fd, resbuf, res_size);
			goto done;
		}
	}
	printf("API code not found\n");

error:
	ret_code = -1;

done:
	printf("Get API code - %d from fd - %d\n", api_code, fd);

	close(fd);
	if (largebuf != NULL && !buf_reused)
		free(largebuf);

	thread_pool[thread_idx].in_used = FALSE;

	return ret_code;
}

int32_t init_server()
{

	int32_t sock_fd, sock_flag;
	int32_t new_sock_fd, thread_idx, ret_code, count;
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

	while (1) {
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
		/* Set thread detached */
		pthread_attr_init(&(thread_pool[thread_idx].attr));
		pthread_attr_setdetachstate(&(thread_pool[thread_idx].attr),
					    PTHREAD_CREATE_DETACHED);
		pthread_create(&(thread_pool[thread_idx].thread),
			       &(thread_pool[thread_idx].attr),
			       (void *)process_request, (void *)thread_idx);
	}

	close(sock_fd);
	return 0;
}

int32_t main()
{
	init_server();
	return 0;
}

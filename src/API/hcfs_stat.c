#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>

#include "global.h"
#include "utils.h"


int _get_usage_val(unsigned int api_code, long long *res_val)
{

	int fd, ret_code, size_msg;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;
	long long ll_ret_code;
	char buf[1];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = api_code;
	cmd_len = 1;
	buf[0] = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ll_ret_code, sizeof(long long), 0);

	if (ll_ret_code < 0) {
		*res_val = 0;
		ret_code = (int)ll_ret_code;
		close(fd);
		return ret_code;
	}

	*res_val = ll_ret_code;
	close(fd);
	return 0;
}

int get_cloud_usage(long long *cloud_usage)
{

	int ret_code;

	ret_code = _get_usage_val(GETCLOUDSIZE, cloud_usage);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

int get_cache_usage(long long *cache_total, long long *cache_used,
		    long long *cache_dirty)
{

	int ret_code;

	ret_code = _get_usage_val(GETMAXCACHESIZE, cache_total);
	if (ret_code < 0)
		return ret_code;

	ret_code = _get_usage_val(GETCACHESIZE, cache_used);
	if (ret_code < 0)
		return ret_code;

	ret_code = _get_usage_val(GETDIRTYCACHESIZE, cache_dirty);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

int get_pin_usage(long long *pin_max, long long *pin_total)
{

	int ret_code;

	ret_code = _get_usage_val(GETMAXPINSIZE, pin_max);
	if (ret_code < 0)
		return ret_code;

	ret_code = _get_usage_val(GETPINSIZE, pin_total);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

int get_xfer_usage(long long *xfer_up, long long *xfer_down)
{

	int fd, ret_code, size_msg;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;
	char buf[1];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETXFERSTAT;
	cmd_len = 1;
	buf[0] = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	if (reply_len > sizeof(int)) {
		size_msg = recv(fd, xfer_down, sizeof(long long), 0);
		size_msg = recv(fd, xfer_up, sizeof(long long), 0);
	} else {
		size_msg = recv(fd, &ret_code, sizeof(int), 0);
		return ret_code;
	}

	close(fd);
	return 0;
}

int get_hcfs_stat(long long *cloud_usage, long long *cache_total,
		  long long *cache_used, long long *cache_dirty,
		  long long *pin_max, long long *pin_total,
		  long long *xfer_up, long long *xfer_down)
{

	int ret_code;

	ret_code = get_cloud_usage(cloud_usage);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_cache_usage(cache_total, cache_used, cache_dirty);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_pin_usage(pin_max, pin_total);
	if (ret_code < 0)
		return ret_code;

	get_xfer_usage(xfer_up, xfer_down);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

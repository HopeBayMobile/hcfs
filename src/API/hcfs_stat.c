/* REVIEW TODO: header for this file */
/* REVIEW TODO: Please run both style checkers after changing the code */

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

/* REVIEW TODO:  According to the coding style we are following, the
header file with this c code should be the first one
to be included (before the standard header files) */
#include "hcfs_stat.h"

#include "global.h"
#include "utils.h"


int32_t _get_usage_val(uint32_t api_code, int64_t *res_val)
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;
	int64_t ll_ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = api_code;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ll_ret_code, sizeof(int64_t), 0);

	if (ll_ret_code < 0) {
		*res_val = 0;
		ret_code = (int32_t)ll_ret_code;
		close(fd);
		return ret_code;
	}

	*res_val = ll_ret_code;
	close(fd);
	return 0;
}

int32_t get_quota(int64_t *quota)
{

	int32_t ret_code;

	ret_code = _get_usage_val(GETQUOTA, quota);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

int32_t get_volume_usage(int64_t *vol_usage)
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;
	int64_t ll_ret_code;
	char buf[1];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETVOLSIZE;
	cmd_len = 1;
	buf[0] = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ll_ret_code, sizeof(int64_t), 0);

	if (ll_ret_code < 0) {
		*vol_usage = 0;
		ret_code = (int32_t)ll_ret_code;
		close(fd);
		return ret_code;
	}

	*vol_usage = ll_ret_code;
	close(fd);
	return 0;
}

int32_t get_cloud_usage(int64_t *cloud_usage)
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;
	int64_t ll_ret_code;
	char buf[1];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETCLOUDSIZE;
	cmd_len = 1;
	buf[0] = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ll_ret_code, sizeof(int64_t), 0);

	if (ll_ret_code < 0) {
		*cloud_usage = 0;
		ret_code = (int32_t)ll_ret_code;
		close(fd);
		return ret_code;
	}

	*cloud_usage = ll_ret_code;
	close(fd);
	return 0;
}

int32_t get_cache_usage(int64_t *cache_total, int64_t *cache_used,
		        int64_t *cache_dirty)
{

	int32_t ret_code;

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

int32_t get_pin_usage(int64_t *pin_max, int64_t *pin_total)
{

	int32_t ret_code;

	ret_code = _get_usage_val(GETMAXPINSIZE, pin_max);
	if (ret_code < 0)
		return ret_code;

	ret_code = _get_usage_val(GETPINSIZE, pin_total);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

int32_t get_xfer_usage(int64_t *xfer_up, int64_t *xfer_down)
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETXFERSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len > sizeof(int32_t)) {
		size_msg = recv(fd, xfer_down, sizeof(int64_t), 0);
		size_msg = recv(fd, xfer_up, sizeof(int64_t), 0);
	} else {
		size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);
		close(fd);
		return ret_code;
	}

	close(fd);
	return 0;
}

int32_t get_cloud_stat(int32_t *cloud_stat)
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CLOUDSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	*cloud_stat = ret_code;
	close(fd);
	return 0;
}

int32_t get_data_transfer(int32_t *data_transfer)
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETXFERSTATUS;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	*data_transfer = ret_code;
	close(fd);
	return 0;
}

int32_t get_hcfs_stat(int64_t *quota, int64_t *vol_usage, int64_t *cloud_usage,
		      int64_t *cache_total, int64_t *cache_used, int64_t *cache_dirty,
		      int64_t *pin_max, int64_t *pin_total,
		      int64_t *xfer_up, int64_t *xfer_down,
		      int32_t *cloud_stat, int32_t *data_transfer)
{

	int32_t ret_code;

	ret_code = get_quota(quota);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_volume_usage(vol_usage);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_cloud_usage(cloud_usage);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_cache_usage(cache_total, cache_used, cache_dirty);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_pin_usage(pin_max, pin_total);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_xfer_usage(xfer_up, xfer_down);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_cloud_stat(cloud_stat);
	if (ret_code < 0)
		return ret_code;

	ret_code = get_data_transfer(data_transfer);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

int32_t get_occupied_size(int64_t *occupied)
{

	int32_t ret_code;

	ret_code = _get_usage_val(OCCUPIEDSIZE, occupied);
	if (ret_code < 0)
		return ret_code;

	return 0;
}


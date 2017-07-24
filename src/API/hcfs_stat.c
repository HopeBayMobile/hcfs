/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_stat.c
* Abstract: This c source file for statistics operations.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#include "hcfs_stat.h"

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>

#include "global.h"
#include "socket_util.h"


/************************************************************************
 * *
 * * Function name: _get_usage_val
 * *        Inputs: uint32_t api_code, int64_t *res_val
 * *       Summary: To get retrun value stored in (res_val) with (api_code).
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t _get_usage_val(uint32_t api_code, int64_t *res_val)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	int64_t ll_ret_code;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = api_code;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ll_ret_code, sizeof(int64_t), 0);

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

/************************************************************************
 * *
 * * Function name: get_quota
 * *        Inputs: int64_t *quota
 * *       Summary: To get value of quota.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_quota(int64_t *quota)
{
	int32_t ret_code;

	ret_code = _get_usage_val(GETQUOTA, quota);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

/************************************************************************
 * *
 * * Function name: get_volume_usage
 * *        Inputs: int64_t *vol_usage
 * *       Summary: To get value of volume usage.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_volume_usage(int64_t *vol_usage)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	int64_t ll_ret_code;
	char buf[1];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETVOLSIZE;
	cmd_len = 1;
	buf[0] = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ll_ret_code, sizeof(int64_t), 0);

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

/************************************************************************
 * *
 * * Function name: get_cloud_usage
 * *        Inputs: int64_t *cloud_usage
 * *       Summary: To get value of cloud usage.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_cloud_usage(int64_t *cloud_usage)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	int64_t ll_ret_code;
	char buf[1];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETCLOUDSIZE;
	cmd_len = 1;
	buf[0] = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ll_ret_code, sizeof(int64_t), 0);

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

int32_t get_max_meta_size(int64_t *max_meta)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	int64_t ll_ret_code;
	char buf[1];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETMAXMETASIZE;
	cmd_len = 1;
	buf[0] = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ll_ret_code, sizeof(int64_t), 0);

	if (ll_ret_code < 0) {
		*max_meta = 0;
		ret_code = (int32_t)ll_ret_code;
		close(fd);
		return ret_code;
	}

	*max_meta = ll_ret_code;
	close(fd);
	return 0;
}



/************************************************************************
 * *
 * * Function name: get_cache_usage
 * *        Inputs: int64_t *cache_total, int64_t *cache_used
 * *		    int64_t *cache_dirty
 * *       Summary: To get values of cache usage. (total; used; dirty)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_cache_usage(int64_t *cache_total, int64_t *cache_used,
		        int64_t *cache_dirty, int64_t *meta_used)
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

	ret_code = _get_usage_val(GETMETASIZE, meta_used);
	if (ret_code < 0)
		return ret_code;

	return 0;
}

/************************************************************************
 * *
 * * Function name: get_pin_usage
 * *        Inputs: int64_t *pin_max, int64_t *pin_total
 * *       Summary: To get values of pin usgae. (max; total)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
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

/************************************************************************
 * *
 * * Function name: get_xfer_usage
 * *        Inputs: int64_t *xfer_up, int64_t *xfer_down
 * *       Summary: To get values of xfer usage. (upload; download)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_xfer_usage(int64_t *xfer_up, int64_t *xfer_down)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETXFERSTAT;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len > sizeof(int32_t)) {
		recv(fd, xfer_down, sizeof(int64_t), 0);
		recv(fd, xfer_up, sizeof(int64_t), 0);
	} else {
		recv(fd, &ret_code, sizeof(int32_t), 0);
		close(fd);
		return ret_code;
	}

	close(fd);
	return 0;
}

/************************************************************************
 * *
 * * Function name: get_cloud_stat
 * *        Inputs: int64_t *quota
 * *       Summary: To get value of cloud stat. (online; offline)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_cloud_stat(int32_t *cloud_stat)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CLOUDSTAT;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	*cloud_stat = ret_code;
	close(fd);
	return 0;
}

/************************************************************************
 * *
 * * Function name: get_data_transfer
 * *        Inputs: int64_t *data_transfer
 * *       Summary: To get status of data transfer.
 * *		    (not int progress; in progress; slow)
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_data_transfer(int32_t *data_transfer)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETXFERSTATUS;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	*data_transfer = ret_code;
	close(fd);
	return 0;
}

/************************************************************************
 * *
 * * Function name: get_hcfs_stat
 * *        Inputs: HCFS_STAT_TYPE *hcfs_stats
 * *       Summary: To get a struct (hcfs_stats) stores hcfs statistics.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_hcfs_stat(HCFS_STAT_TYPE *hcfs_stats)
{
	int32_t ret_code;

	ret_code = get_quota(&(hcfs_stats->quota));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_volume_usage(&(hcfs_stats->vol_usage));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_cloud_usage(&(hcfs_stats->cloud_usage));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_cache_usage(&(hcfs_stats->cache_total),
				   &(hcfs_stats->cache_used),
				   &(hcfs_stats->cache_dirty),
				   &(hcfs_stats->meta_used_size));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_pin_usage(&(hcfs_stats->pin_max),
				 &(hcfs_stats->pin_total));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_xfer_usage(&(hcfs_stats->xfer_up),
				  &(hcfs_stats->xfer_down));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_cloud_stat(&(hcfs_stats->cloud_stat));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_max_meta_size(&(hcfs_stats->max_meta_size));
	if (ret_code < 0)
		return ret_code;

	ret_code = get_data_transfer(&(hcfs_stats->data_transfer));
	if (ret_code < 0)
		return ret_code;

	return 0;
}

/************************************************************************
 * *
 * * Function name: get_occupied_size
 * *        Inputs: int64_t *occupied
 * *       Summary: To get value of occupied size (unpin but dirty + pin size).
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_occupied_size(int64_t *occupied)
{
	int32_t ret_code;

	ret_code = _get_usage_val(OCCUPIEDSIZE, occupied);
	if (ret_code < 0)
		return ret_code;

	return 0;
}


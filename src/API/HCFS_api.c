/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: HCFS_api.c
* Abstract: This c source file for entry point of HCFSAPI.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#include "HCFS_api.h"

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <jansson.h>

#include "pin_ops.h"
#include "global.h"
#include "marco.h"
#include "utils.h"


/* REVIEW TODO: error handling for the jansson lib functions may be needed */
/* For example, json_object_set_new will return -1 if an error occurs */
void _json_response(char **json_str, char result, int32_t code, json_t *data)
{

	json_t *res_obj;

	res_obj = json_object();
	json_object_set_new(res_obj, "result", json_boolean(result));
	json_object_set_new(res_obj, "code", json_integer(code));
	if (data == NULL)
		json_object_set_new(res_obj, "data", json_object());
	else
		json_object_set_new(res_obj, "data", data);

	*json_str = json_dumps(res_obj, JSON_COMPACT);

	json_decref(res_obj);
}

int32_t _api_socket_conn()
{

	int32_t fd, status;
	struct sockaddr_un addr;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, API_SOCK_PATH);
/* REVIEW TODO: socket function call might fail, so error handling is required here */
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
/* REVIEW TODO: Could errno change during return? If so, perhaps it is a good idea
to first assign the value to some other variable */
	if (status < 0)
		return -errno;

	return fd;
}

void HCFS_set_config(char **json_res, char *key, char *value)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len;
	ssize_t str_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SETCONFIG;
	cmd_len = 0;

	CONCAT_ARGS(key);
	CONCAT_ARGS(value)

/* REVIEW TODO: perhaps can remove size_msg here if not used (or maybe add error handling?) */
/* Same in the next function */
	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_get_config(char **json_res, char *key)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len;
	ssize_t str_len;
	char buf[1000];
	char value[500];
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = GETCONFIG;
	cmd_len = 0;

	CONCAT_ARGS(key);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);
		if (ret_code == 1) {
			data = json_object();
			json_object_set_new(data, key, json_string(""));

			_json_response(json_res, TRUE, 0, data);
		} else {
			_json_response(json_res, FALSE, -ret_code, NULL);
		}
	} else {
		size_msg = recv(fd, value, reply_len, 0);
		value[reply_len] = 0;

		data = json_object();
		json_object_set_new(data, key, json_string(value));

		_json_response(json_res, TRUE, 0, data);
	}

	close(fd);
}

void HCFS_stat(char **json_res)
{

	int32_t fd, status, size_msg, ret_code;
	int32_t cloud_stat, data_transfer;
	uint32_t code, reply_len, cmd_len, buf_idx;
	int64_t quota, vol_usage, cloud_usage;
	int64_t cache_total, cache_used, cache_dirty;
	int64_t pin_max, pin_total;
	int64_t xfer_up, xfer_down;
	char buf[512];
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = GETSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		size_msg = recv(fd, buf, reply_len, 0);
		buf_idx = 0;

/* REVIEW TODO: Is it possible to use common structure to send / read a collection
of variables, instead of sending them one by one? */
		READ_LL_ARGS(quota);
		READ_LL_ARGS(vol_usage);
		READ_LL_ARGS(cloud_usage);
		READ_LL_ARGS(cache_total);
		READ_LL_ARGS(cache_used);
		READ_LL_ARGS(cache_dirty);
		READ_LL_ARGS(pin_max);
		READ_LL_ARGS(pin_total);
		READ_LL_ARGS(xfer_up);
		READ_LL_ARGS(xfer_down);

		memcpy(&cloud_stat, &(buf[buf_idx]), sizeof(int32_t));
		buf_idx += sizeof(int32_t);

		memcpy(&data_transfer, &(buf[buf_idx]), sizeof(int32_t));
		buf_idx += sizeof(int32_t);

		data = json_object();
		json_object_set_new(data, "quota", json_integer(quota));
		json_object_set_new(data, "vol_used", json_integer(vol_usage));
		json_object_set_new(data, "cloud_used", json_integer(cloud_usage));
		json_object_set_new(data, "cache_total", json_integer(cache_total));
		json_object_set_new(data, "cache_used", json_integer(cache_used));
		json_object_set_new(data, "cache_dirty", json_integer(cache_dirty));
		json_object_set_new(data, "pin_max", json_integer(pin_max));
		json_object_set_new(data, "pin_total", json_integer(pin_total));
		json_object_set_new(data, "xfer_up", json_integer(xfer_up));
		json_object_set_new(data, "xfer_down", json_integer(xfer_down));
		json_object_set_new(data, "cloud_conn", json_boolean(cloud_stat));
		json_object_set_new(data, "data_transfer", json_integer(data_transfer));

		_json_response(json_res, TRUE, 0, data);
	}

	close(fd);
}

void HCFS_get_occupied_size(char **json_res)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len, buf_idx;
	int64_t occupied;
	char buf[512];
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = OCCUPIEDSIZE;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		size_msg = recv(fd, buf, reply_len, 0);
		buf_idx = 0;

		READ_LL_ARGS(occupied);

		data = json_object();
		json_object_set_new(data, "occupied", json_integer(occupied));

		_json_response(json_res, TRUE, 0, data);
	}

	close(fd);
}

void HCFS_reload_config(char **json_res)
{

	int32_t fd, size_msg, ret_code;
	uint32_t code, cmd_len, reply_len;
	ssize_t str_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = RELOADCONFIG;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_toggle_sync(char **json_res, int32_t enabled)
{

	int32_t fd, size_msg, ret_code;
	uint32_t code, cmd_len, reply_len;
	ssize_t str_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SETSYNCSWITCH;
	cmd_len = sizeof(int32_t);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, &enabled, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_get_sync_status(char **json_res)
{

	int32_t fd, size_msg, ret_code;
	uint32_t code, cmd_len, reply_len;
	ssize_t str_len;
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = GETSYNCSWITCH;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0) {
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		data = json_object();
		json_object_set_new(data, "enabled", json_boolean(ret_code));

		_json_response(json_res, TRUE, 0, data);
	}

	close(fd);
}

void HCFS_pin_path(char **json_res, char *pin_path)
{

	int32_t fd, size_msg, ret_code;
	uint32_t code, cmd_len, reply_len;
	ssize_t str_len;
	char buf[500];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = PIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_ARGS(pin_path);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_pin_app(char **json_res, char *app_path, char *data_path,
		  char *sd0_path, char *sd1_path)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len;
	ssize_t str_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = PIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_ARGS(app_path)
	CONCAT_ARGS(data_path)
	CONCAT_ARGS(sd0_path)
	CONCAT_ARGS(sd1_path)

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_unpin_path(char **json_res, char *pin_path)
{

	int32_t fd, size_msg, ret_code;
	uint32_t code, cmd_len, reply_len;
	ssize_t str_len;
	char buf[500];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = UNPIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_ARGS(pin_path);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}


void HCFS_unpin_app(char **json_res, char *app_path, char *data_path,
		    char *sd0_path, char *sd1_path)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len;
	ssize_t str_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = UNPIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_ARGS(app_path)
	CONCAT_ARGS(data_path)
	CONCAT_ARGS(sd0_path)
	CONCAT_ARGS(sd1_path)

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_pin_status(char **json_res, char *pathname)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKPIN;
	cmd_len = strlen(pathname) + 1;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, pathname, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_dir_status(char **json_res, char *pathname)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len, buf_idx;
	int64_t num_local, num_cloud, num_hybrid;
	char buf[3*sizeof(int64_t)];
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKDIRSTAT;
	cmd_len = strlen(pathname) + 1;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, pathname, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		size_msg = recv(fd, buf, reply_len, 0);
		buf_idx = 0;

		READ_LL_ARGS(num_local);
		READ_LL_ARGS(num_cloud);
		READ_LL_ARGS(num_hybrid);

		data = json_object();
		json_object_set_new(data, "num_local", json_integer(num_local));
		json_object_set_new(data, "num_cloud", json_integer(num_cloud));
		json_object_set_new(data, "num_hybrid", json_integer(num_hybrid));

		_json_response(json_res, TRUE, 0, data);
	}

	close(fd);
}

void HCFS_file_status(char **json_res, char *pathname)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKLOC;
	cmd_len = strlen(pathname) + 1;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, pathname, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_reset_xfer(char **json_res)
{

	int32_t fd, status, size_msg, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = RESETXFERSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}


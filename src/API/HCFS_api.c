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

#include "HCFS_api.h"

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <jansson.h>

#include "pin_ops.h"
#include "global.h"
#include "marco.h"
#include "socket_util.h"
#include "hcfs_stat.h"

void _json_response(char **json_str, char result, int32_t code, json_t *data)
{
	json_t *res_obj;

	res_obj = json_object();
	if (res_obj == NULL)
		goto error;

	JSON_OBJ_SET_NEW(res_obj, "result", json_boolean(result));
	JSON_OBJ_SET_NEW(res_obj, "code", json_integer(code));
	if (data == NULL) {
		JSON_OBJ_SET_NEW(res_obj, "data", json_object());
	} else {
		JSON_OBJ_SET_NEW(res_obj, "data", data);
	}

	*json_str = json_dumps(res_obj, JSON_COMPACT);

	goto end;

error:
	json_decref(data);
	*json_str = NULL;
end:
	json_decref(res_obj);
	return;
}

int32_t _api_socket_conn()
{
	int32_t fd, status;
	struct sockaddr_un addr;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, API_SOCK_PATH);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
		return -errno;
	status = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (status < 0)
		return -errno;

	return fd;
}

void _send_apicode_without_args(int32_t api_code, char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = api_code;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_set_config(char **json_res, char *key, char *value)
{
	int32_t fd, ret_code;
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
	CONCAT_ARGS(value);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_get_config(char **json_res, char *key)
{
	int32_t fd, ret_code;
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

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		recv(fd, &ret_code, sizeof(int32_t), 0);
		if (ret_code == 1) {
			data = json_object();
			if (data == NULL)
				goto error;
			JSON_OBJ_SET_NEW(data, key, json_string(""));

			_json_response(json_res, TRUE, 0, data);
		} else {
			_json_response(json_res, FALSE, -ret_code, NULL);
		}
	} else {
		recv(fd, value, reply_len, 0);
		value[reply_len] = 0;

		data = json_object();
		if (data == NULL)
			goto error;
		JSON_OBJ_SET_NEW(data, key, json_string(value));

		_json_response(json_res, TRUE, 0, data);
	}

	goto end;

error:
	json_decref(data);
	*json_res = NULL;
end:
	close(fd);
	return;
}

void HCFS_stat(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;
	HCFS_STAT_TYPE hcfs_stats;
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = GETSTAT;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		recv(fd, &ret_code, sizeof(int32_t), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		recv(fd, &hcfs_stats, reply_len, 0);

		data = json_object();
		if (data == NULL)
			goto error;
		JSON_OBJ_SET_NEW(data, "quota", json_integer(hcfs_stats.quota));
		JSON_OBJ_SET_NEW(data, "vol_used",
				 json_integer(hcfs_stats.vol_usage));
		JSON_OBJ_SET_NEW(data, "cloud_used",
				 json_integer(hcfs_stats.cloud_usage));
		JSON_OBJ_SET_NEW(data, "cache_total",
				 json_integer(hcfs_stats.cache_total));
		JSON_OBJ_SET_NEW(data, "cache_used",
				 json_integer(hcfs_stats.cache_used));
		JSON_OBJ_SET_NEW(data, "cache_dirty",
				 json_integer(hcfs_stats.cache_dirty));
		JSON_OBJ_SET_NEW(data, "pin_max",
				 json_integer(hcfs_stats.pin_max));
		JSON_OBJ_SET_NEW(data, "pin_total",
				 json_integer(hcfs_stats.pin_total));
		JSON_OBJ_SET_NEW(data, "xfer_up",
				 json_integer(hcfs_stats.xfer_up));
		JSON_OBJ_SET_NEW(data, "xfer_down",
				 json_integer(hcfs_stats.xfer_down));
		JSON_OBJ_SET_NEW(data, "cloud_conn",
				 json_integer(hcfs_stats.cloud_stat));
		JSON_OBJ_SET_NEW(data, "data_transfer",
				 json_integer(hcfs_stats.data_transfer));
		JSON_OBJ_SET_NEW(data, "max_meta_size",
				 json_integer(hcfs_stats.max_meta_size));
		JSON_OBJ_SET_NEW(data, "meta_used_size",
				 json_integer(hcfs_stats.meta_used_size));

		_json_response(json_res, TRUE, 0, data);
	}

	goto end;

error:
	json_decref(data);
	*json_res = NULL;
end:
	close(fd);
	return;
}

void HCFS_get_occupied_size(char **json_res)
{
	int32_t fd, ret_code;
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

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		recv(fd, &ret_code, sizeof(int32_t), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		recv(fd, buf, reply_len, 0);
		buf_idx = 0;

		READ_LL_ARGS(occupied);

		data = json_object();
		if (data == NULL)
			goto error;
		JSON_OBJ_SET_NEW(data, "occupied", json_integer(occupied));

		_json_response(json_res, TRUE, 0, data);
	}

	goto end;

error:
	json_decref(data);
	*json_res = NULL;
end:
	close(fd);
	return;
}

void HCFS_reload_config(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = RELOADCONFIG;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_toggle_sync(char **json_res, int32_t enabled)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SETSYNCSWITCH;
	cmd_len = sizeof(int32_t);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, &enabled, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_get_sync_status(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = GETSYNCSWITCH;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0) {
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		data = json_object();
		if (data == NULL)
			goto error;
		JSON_OBJ_SET_NEW(data, "enabled", json_boolean(ret_code));

		_json_response(json_res, TRUE, 0, data);
	}

	goto end;

error:
	json_decref(data);
	*json_res = NULL;
end:
	close(fd);
	return;
}

void HCFS_pin_path(char **json_res, char *pin_path, char pin_type)
{
	int32_t fd, ret_code;
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

	/* Pin type */
	memcpy(&buf[0], &pin_type, sizeof(char));
	cmd_len += sizeof(char);

	/* Append paths to socket msg */
	CONCAT_ARGS(pin_path);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_unpin_path(char **json_res, char *pin_path)
{
	int32_t fd, ret_code;
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

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_pin_status(char **json_res, char *pathname)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKPIN;
	cmd_len = strlen(pathname) + 1;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, pathname, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_dir_status(char **json_res, char *pathname)
{
	int32_t fd, ret_code;
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

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, pathname, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	if (reply_len == 0) {
		recv(fd, &ret_code, sizeof(int32_t), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		recv(fd, buf, reply_len, 0);
		buf_idx = 0;

		READ_LL_ARGS(num_local);
		READ_LL_ARGS(num_cloud);
		READ_LL_ARGS(num_hybrid);

		data = json_object();
		if (data == NULL)
			goto error;
		JSON_OBJ_SET_NEW(data, "num_local", json_integer(num_local));
		JSON_OBJ_SET_NEW(data, "num_cloud", json_integer(num_cloud));
		JSON_OBJ_SET_NEW(data, "num_hybrid", json_integer(num_hybrid));

		_json_response(json_res, TRUE, 0, data);
	}

	goto end;

error:
	json_decref(data);
	*json_res = NULL;
end:
	close(fd);
	return;
}

void HCFS_file_status(char **json_res, char *pathname)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKLOC;
	cmd_len = strlen(pathname) + 1;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, pathname, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_reset_xfer(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = RESETXFERSTAT;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_set_notify_server(char **json_res, char *path)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SETNOTIFYSERVER;
	cmd_len = strlen(path) + 1;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, path, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_set_swift_token(char **json_res, char *url, char *token)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;
	ssize_t str_len;
	char buf[4096];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SETSWIFTTOKEN;
	cmd_len = 0;

	CONCAT_ARGS(url);
	CONCAT_ARGS(token);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_set_sync_point(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SETSYNCPOINT;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_clear_sync_point(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CANCELSYNCPOINT;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_trigger_restore(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = INITIATE_RESTORATION;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_check_restore_status(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECK_RESTORATION_STATUS;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_collect_sys_logs(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = COLLECTSYSLOGS;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_notify_applist_change(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = NOTIFY_APPLIST_CHANGE;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_check_package_boost_status(char **json_res, char *package_name)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	ssize_t str_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECK_PACKAGE_BOOST_STATUS;
	cmd_len = 0;

	CONCAT_ARGS(package_name);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_enable_booster(char **json_res, int64_t size)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = ENABLE_BOOSTER;
	cmd_len = sizeof(int64_t);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, &size, sizeof(int64_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_disable_booster(char **json_res)
{
	_json_response(json_res, FALSE, -ENOTSUP, NULL);
}

void HCFS_trigger_boost(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = TRIGGER_BOOST;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_trigger_unboost(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = TRIGGER_UNBOOST;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_clear_booster_package_remaining(char **json_res, char *package_name)
{
	int32_t fd, ret_code;
	uint32_t code, reply_len, cmd_len;
	ssize_t str_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CLEAR_BOOSTED_PACKAGE;
	cmd_len = 0;

	CONCAT_ARGS(package_name);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_mount_smart_cache(char **json_res)
{
	_send_apicode_without_args(MOUNT_SMART_CACHE, json_res);
}

void HCFS_umount_smart_cache(char **json_res)
{
	_send_apicode_without_args(UMOUNT_SMART_CACHE, json_res);
}

void HCFS_create_minimal_apk(char **json_res,
			     char *package_name,
			     int32_t blocking,
			     int num_icon,
			     char *icon_name_list)
{
	int32_t fd, ret_code, i;
	uint32_t code, cmd_len, reply_len, now_pos = 0;
	ssize_t str_len;
	char buf[5000] = {0};

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CREATE_MINI_APK;
	cmd_len = 0;

	memcpy(buf, &blocking, sizeof(int32_t));
	cmd_len += sizeof(int32_t);
	CONCAT_ARGS(package_name);

	/* Concat icon name array */
	memcpy(buf + cmd_len, &num_icon, sizeof(int32_t));
	cmd_len += sizeof(int32_t);
	now_pos = 0;
	for (i = 0; i < num_icon; i++) {
		// icon_name_list is a icon name array that each element is
		// separated by null character.
		CONCAT_ARGS(icon_name_list + now_pos);
		now_pos += (strlen(icon_name_list + now_pos) + 1);
	}

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_retry_conn(char **json_res)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = RETRY_CONN;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}

void HCFS_check_minimal_apk(char **json_res, char *package_name)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	ssize_t str_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECK_MINI_APK;
	cmd_len = 0;

	CONCAT_ARGS(package_name);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

	close(fd);
}


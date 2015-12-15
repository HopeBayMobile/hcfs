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

#include "HCFS_api.h"

#include "pin_ops.h"
#include "global.h"
#include "marco.h"
#include "utils.h"


void _json_response(char **json_str, char result, int code, json_t *data)
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

int _api_socket_conn()
{

	int fd, status;
	struct sockaddr_un addr;

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, API_SOCK_PATH);
	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	status = connect(fd, &addr, sizeof(addr));
	if (status < 0)
		return -errno;

	return fd;
}

void HCFS_set_config(char **json_res, char *key, char *value)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;
	ssize_t path_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SETCONFIG;
	cmd_len = 0;

	CONCAT_PIN_ARG(key);
	CONCAT_PIN_ARG(value)

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}

void HCFS_get_config(char **json_res, char *key)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;
	ssize_t path_len;
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

	CONCAT_PIN_ARG(key);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);
		if (ret_code == 1) {
			data = json_object();
			json_object_set_new(data, key, json_string(""));

			_json_response(json_res, TRUE, 0, data);
			json_decref(data);
		} else {
			_json_response(json_res, FALSE, -ret_code, NULL);
		}
	} else {
		size_msg = recv(fd, value, reply_len, 0);
		value[reply_len] = 0;

		data = json_object();
		json_object_set_new(data, key, json_string(value));

		_json_response(json_res, TRUE, 0, data);
		json_decref(data);
	}
}

void HCFS_stat(char **json_res)
{

	int fd, status, size_msg, ret_code;
	int cloud_stat;
	unsigned int code, reply_len, cmd_len, buf_idx;
	long long cloud_usage;
	long long cache_total, cache_used, cache_dirty;
	long long pin_max, pin_total;
	long long xfer_up, xfer_down;
	char buf[512];
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = GETSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		ret_code = 0;
		size_msg = recv(fd, buf, reply_len, 0);

		buf_idx = 0;
		memcpy(&cloud_usage, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&cache_total, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&cache_used, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&cache_dirty, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&pin_max, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&pin_total, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&xfer_up, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&xfer_down, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&cloud_stat, &(buf[buf_idx]), sizeof(int));
		buf_idx += sizeof(int);

		data = json_object();
		json_object_set_new(data, "cloud_used", json_integer(cloud_usage));
		json_object_set_new(data, "cache_total", json_integer(cache_total));
		json_object_set_new(data, "cache_used", json_integer(cache_used));
		json_object_set_new(data, "cache_dirty", json_integer(cache_dirty));
		json_object_set_new(data, "pin_max", json_integer(pin_max));
		json_object_set_new(data, "pin_total", json_integer(pin_total));
		json_object_set_new(data, "xfer_up", json_integer(xfer_up));
		json_object_set_new(data, "xfer_down", json_integer(xfer_down));
		json_object_set_new(data, "cloud_conn", json_boolean(cloud_stat));

		_json_response(json_res, TRUE, ret_code, data);
		json_decref(data);
	}
}

void HCFS_pin_path(char **json_res, char *pin_path)
{

	int fd, size_msg, ret_code;
	unsigned int code, cmd_len, reply_len;
	ssize_t path_len;
	char buf[500];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = PIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_PIN_ARG(pin_path);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}

void HCFS_pin_app(char **json_res, char *app_path, char *data_path,
		  char *sd0_path, char *sd1_path)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;
	ssize_t path_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = PIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_PIN_ARG(app_path)
	CONCAT_PIN_ARG(data_path)
	CONCAT_PIN_ARG(sd0_path)
	CONCAT_PIN_ARG(sd1_path)

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

}

void HCFS_unpin_path(char **json_res, char *pin_path)
{

	int fd, size_msg, ret_code;
	unsigned int code, cmd_len, reply_len;
	ssize_t path_len;
	char buf[500];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = UNPIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_PIN_ARG(pin_path);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}


void HCFS_unpin_app(char **json_res, char *app_path, char *data_path,
		    char *sd0_path, char *sd1_path)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;
	ssize_t path_len;
	char buf[1000];

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = UNPIN;
	cmd_len = 0;

	/* Append paths to socket msg */
	CONCAT_PIN_ARG(app_path)
	CONCAT_PIN_ARG(data_path)
	CONCAT_PIN_ARG(sd0_path)
	CONCAT_PIN_ARG(sd1_path)

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

}

void HCFS_pin_status(char **json_res, char *pathname)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKPIN;
	cmd_len = strlen(pathname);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, pathname, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}

void HCFS_dir_status(char **json_res, char *pathname)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len, buf_idx;
	long long num_local, num_cloud, num_hybrid;
	char buf[3*sizeof(long long)];
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKDIRSTAT;
	cmd_len = strlen(pathname);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, pathname, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		ret_code = 0;
		size_msg = recv(fd, buf, reply_len, 0);

		memcpy(&num_local, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&num_cloud, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		memcpy(&num_hybrid, &(buf[buf_idx]), sizeof(long long));
		buf_idx += sizeof(long long);

		data = json_object();
		json_object_set_new(data, "num_local", json_integer(num_local));
		json_object_set_new(data, "num_cloud", json_integer(num_cloud));
		json_object_set_new(data, "num_hybrid", json_integer(num_hybrid));

		_json_response(json_res, TRUE, ret_code, data);
		json_decref(data);
	}
}

void HCFS_file_status(char **json_res, char *pathname)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = CHECKLOC;
	cmd_len = strlen(pathname);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, pathname, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}

void HCFS_reset_xfer(char **json_res)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = RESETXFERSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}

void HCFS_reboot(char **json_res)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = SYSREBOOT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	_json_response(json_res, TRUE, ret_code, NULL);
}

void HCFS_get_pkg_uid(char **json_res, char *pkg_name)
{

	int fd, status, size_msg, ret_code;
	unsigned int code, reply_len, cmd_len;
	ssize_t path_len;
	char buf[1000];
	char value[500];
	json_t *data;

	fd = _api_socket_conn();
	if (fd < 0) {
		_json_response(json_res, FALSE, -fd, NULL);
		return;
	}

	code = QUERYPKGUID;

	CONCAT_PIN_ARG(pkg_name);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, buf, cmd_len, 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	if (reply_len == 0) {
		size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);
		_json_response(json_res, FALSE, -ret_code, NULL);
	} else {
		size_msg = recv(fd, value, reply_len, 0);
		value[reply_len] = 0;
		data = json_object();
		json_object_set_new(data, "uid", json_string(value));

		_json_response(json_res, TRUE, 0, data);
		json_decref(data);
	}

}

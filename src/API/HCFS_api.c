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


int _json_response(char *json_str, char result, int code, json_t *data)
{

	char *res_str;
	json_t *res_obj;

	res_obj = json_object();
	json_object_set_new(res_obj, "result", json_boolean(result));
	json_object_set_new(res_obj, "code", json_integer(code));
	json_object_set_new(res_obj, "data", json_object());

	res_str = json_dumps(res_obj, JSON_COMPACT);
	sprintf(json_str, res_str);

	free(res_str);
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

void HCFS_pin_path(char *json_res, char *pin_path)
{

	int fd, code, size_msg, ret_code;
	unsigned int cmd_len, reply_len;
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

	//size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}

void HCFS_pin_app(char *json_res, char *app_path, char *data_path,
		  char *sd0_path, char *sd1_path)
{

	int num_path;
	int fd, code, status, size_msg, ret_code;
	unsigned int reply_len, cmd_len;
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

	//size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

}

void HCFS_unpin_path(char *json_res, char *pin_path)
{

	int fd, code, size_msg, ret_code;
	unsigned int cmd_len, reply_len;
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

	//size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);
}


void HCFS_unpin_app(char *json_res, char *app_path, char *data_path,
		    char *sd0_path, char *sd1_path)
{

	int num_path;
	int fd, code, status, size_msg, ret_code;
	unsigned int reply_len, cmd_len;
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

	//size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(unsigned int), 0);

	if (ret_code < 0)
		_json_response(json_res, FALSE, -ret_code, NULL);
	else
		_json_response(json_res, TRUE, ret_code, NULL);

}

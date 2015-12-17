#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sqlite3.h>

#include "hcfs_sys.h"

#include "global.h"
#include "utils.h"


int _validate_config_key(char *key)
{

	int idx, num_keys;
	char *keys[] = {\
		"current_backend",\
		"swift_account",\
		"swift_user",\
		"swift_pass",\
		"swift_url",\
		"swift_container",\
		"swift_protocol"\
	};


	num_keys = sizeof(keys) / sizeof(keys[0]);

	for (idx = 0; idx < num_keys; idx++) {
		if (strcmp(keys[idx], key) == 0)
			return 0;
	}
	return -1;
}

int set_hcfs_config(char *arg_buf, unsigned int arg_len)
{

	int idx, ret_code;
	unsigned int msg_len;
	ssize_t str_len;
	char tmp_path[100];
	char buf[300];
	char key[100], upper_key[100], value[400];
	char *tmp_ptr, *line, *token;
	FILE *conf, *tmp_conf;


	msg_len = 0;
	str_len = 0;
	memcpy(&str_len, &(arg_buf[msg_len]), sizeof(ssize_t));
	msg_len += sizeof(ssize_t);

	memcpy(key, &(arg_buf[msg_len]), str_len);
	key[str_len] = 0;
	msg_len += str_len;

	memcpy(&str_len, &(arg_buf[msg_len]), sizeof(ssize_t));
	msg_len += sizeof(ssize_t);

	memcpy(value, &(arg_buf[msg_len]), str_len);
	value[str_len] = 0;
	msg_len += str_len;

	if (msg_len != arg_len)
		return -EINVAL;

	ret_code = _validate_config_key(key);
	if (ret_code < 0) {
		return -1;
	}

	memcpy(upper_key, key, sizeof(upper_key));
	for (idx = 0; idx < strlen(key); idx++)
		upper_key[idx] = toupper(upper_key[idx]);

	snprintf(tmp_path, sizeof(tmp_path),"%s.%s",
		CONF_PATH, "tmp");

	conf = fopen(CONF_PATH, "r");
	if (conf == NULL) {
		return -errno;
	}

	tmp_conf = fopen(tmp_path, "w");
	if (tmp_conf == NULL) {
		fclose(conf);
		return -errno;
	}

	while (fgets(buf, sizeof(buf), conf) != NULL) {
		tmp_ptr = line = strdup(buf);
		token = strsep(&line, " = ");
		if (strcmp(upper_key, token) == 0)
			snprintf(buf, sizeof(buf), "%s = %s\n", token, value);
		fputs(buf, tmp_conf);
		free(tmp_ptr);
	}

	fclose(conf);
	fclose(tmp_conf);

	ret_code = rename(tmp_path, CONF_PATH);
	if (ret_code < 0)
		return -errno;
	else
		return 0;
}

int get_hcfs_config(char *arg_buf, unsigned int arg_len, char **value)
{

	int idx, ret_code;
	unsigned int msg_len;
	ssize_t str_len;
	char tmp_path[100];
	char buf[300];
	char key[100], upper_key[100];
	char *tmp_ptr, *line, *token;
	FILE *conf;


	msg_len = 0;
	str_len = 0;
	memcpy(&str_len, &(arg_buf[msg_len]), sizeof(ssize_t));
	msg_len += sizeof(ssize_t);

	memcpy(key, &(arg_buf[msg_len]), str_len);
	key[str_len] = 0;
	msg_len += str_len;

	printf("key is %s\n", key);

	if (msg_len != arg_len) {
		printf("Arg len is different\n");
		return -EINVAL;
	}

	ret_code = _validate_config_key(key);
	if (ret_code < 0) {
		printf("Invalid key\n");
		return -1;
	}

	memcpy(upper_key, key, sizeof(upper_key));
	for (idx = 0; idx < strlen(key); idx++)
		upper_key[idx] = toupper(upper_key[idx]);

	conf = fopen(CONF_PATH, "r");
	if (conf == NULL) {
	        return -errno;
	}

	ret_code = -1;
	while (fgets(buf, sizeof(buf), conf) != NULL) {
		tmp_ptr = line = strdup(buf);
		token = strsep(&line, " ");
		if (strcmp(upper_key, token) == 0) {
			token = strsep(&line, " =");
			if (strlen(line) <= 1) {
				ret_code = 1;
				free(tmp_ptr);
				break;
			}

			token = strsep(&line, " ");
			token = strsep(&line, "\n");
			if (strlen(token) <= 0) {
				ret_code = 1;
			} else {
				*value = malloc((strlen(token) + 1) * sizeof(char));
				strcpy(*value, token);
				ret_code = 0;
			}
			free(tmp_ptr);
			break;
		}
		free(tmp_ptr);
	}
	fclose(conf);

	if (ret_code < 0)
		return -EINVAL;
	else
		return ret_code;
}

int reload_hcfs_config()
{

	int fd, ret_code, size_msg;
	unsigned int code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = RELOADCONFIG;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	close(fd);

	return ret_code;
}

int toggle_cloud_sync(char *arg_buf, unsigned int arg_len)
{

	int fd, ret_code, size_msg, enabled;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;

	memcpy(&enabled, arg_buf, sizeof(int));
	if (enabled != 0 && enabled != 1)
		return -EINVAL;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = SETSYNCSWITCH;
	cmd_len = sizeof(int);

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);
	size_msg = send(fd, &enabled, sizeof(int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	close(fd);

	return ret_code;
}

int get_sync_status()
{

	int fd, ret_code, size_msg;
	unsigned int code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETSYNCSWITCH;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	close(fd);

	return ret_code;
}

int reset_xfer_usage()
{

	int fd, ret_code, size_msg;
	unsigned int code, cmd_len, reply_len, total_recv, to_recv;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = RESETXFERSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(unsigned int), 0);
	size_msg = send(fd, &cmd_len, sizeof(unsigned int), 0);

	size_msg = recv(fd, &reply_len, sizeof(unsigned int), 0);
	size_msg = recv(fd, &ret_code, sizeof(int), 0);

	close(fd);

	return ret_code;
}

/* Callback function for sql statement */
static int _sqlite_exec_cb(void *data, int argc, char **argv, char **azColName)
{

	size_t uid_len;
	char **uid = (char **)data;

	uid_len = argv[0] ? strlen(argv[0]) : strlen("NULL");
	*uid = malloc(sizeof(char) * (uid_len + 1));
	snprintf(*uid, uid_len + 1, "%s", argv[0]);

	return 0;
}

int query_pkg_uid(char *arg_buf, unsigned int arg_len, char **uid)
{

	int idx, ret_code;
	unsigned int msg_len;
	ssize_t str_len;
	char pkg_name[400];
	char sql[1000];
	sqlite3 *db;
	char *sql_err = 0;


	msg_len = 0;
	str_len = 0;
	memcpy(&str_len, &(arg_buf[msg_len]), sizeof(ssize_t));
	msg_len += sizeof(ssize_t);

	memcpy(pkg_name, &(arg_buf[msg_len]), str_len);
	pkg_name[str_len] = 0;
	msg_len += str_len;

	printf("pkg name is %s\n", pkg_name);

	if (msg_len != arg_len) {
		printf("Arg len is different\n");
		return -EINVAL;
	}

	snprintf(sql, sizeof(sql), "SELECT uid from uid WHERE package_name='%s'", pkg_name);

	ret_code = sqlite3_open(DB_PATH, &db);
	if (ret_code != 0) {
	        printf("Failed to open sqlite db - err is %s\n", sqlite3_errmsg(db));
		return ret_code;
	}

	ret_code = sqlite3_exec(db, sql, _sqlite_exec_cb, (void *)uid, &sql_err);
	if( ret_code != SQLITE_OK ){
		printf("Failed to execute sql statement - err is %s\n", sql_err);
		sqlite3_free(sql_err);
	}

	sqlite3_close(db);

	if (ret_code == 0 && *uid == NULL)
		return -ENOENT;
	else
		return ret_code;
}

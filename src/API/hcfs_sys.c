#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "hcfs_sys.h"

#include "global.h"
#include "utils.h"


int _validate_config_key(char *key)
{

	int idx, num_keys;
	char *keys[] = {\
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
	msg_len += str_len;

	memcpy(&str_len, &(arg_buf[msg_len]), sizeof(ssize_t));
	msg_len += sizeof(ssize_t);

	memcpy(value, &(arg_buf[msg_len]), str_len);
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
		token = strsep(&line, " =");
		if (strcmp(upper_key, token) == 0) {
			token = strsep(&line, " =");
			token = strsep(&line, " ");
			token = strsep(&line, "\n");

			*value = malloc(strlen(token)*sizeof(char));
			strcpy(*value, token);

			free(tmp_ptr);
			ret_code = 0;
			break;
		}

		free(tmp_ptr);
	}

	fclose(conf);

	if (ret_code < 0)
		return -EINVAL;
	else
		return 0;
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

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sqlite3.h>
#include <inttypes.h>

#include "hcfs_sys.h"

#include "global.h"
#include "utils.h"
#include "enc.h"


int32_t _validate_config_key(char *key)
{

	int32_t idx, num_keys;
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

/*
 * NOTE:
 *     The size of *output* should be larger than
 *     input_len + IV_SIZE + TAG_SIZE.
 */
int32_t _encrypt_config(uint8_t *output, uint8_t *input,
		   int64_t input_len)
{

	int32_t ret;
	uint8_t iv[IV_SIZE] = {0};
	uint8_t *enc_key, *enc_data;
	int64_t data_size, enc_size;

	/* Key and iv */
	enc_key = get_key(PASSPHRASE);
	generate_random_bytes(iv, IV_SIZE);

	enc_data = (char*)malloc(
			sizeof(uint8_t) * (input_len + TAG_SIZE));

	ret = aes_gcm_encrypt_core(enc_data, input, input_len, enc_key, iv);
	free(enc_key);
	if (ret != 0) {
		free(enc_data);
		return -1;
	}

	memcpy(output, iv, IV_SIZE);
	memcpy(output + IV_SIZE, enc_data, input_len + TAG_SIZE);

	free(enc_data);
	return 0;
}

FILE *_get_decrypt_configfp()
{

	int64_t file_size, enc_size, data_size;
	FILE *datafp = NULL;
	uint8_t *iv_buf = NULL;
        uint8_t *enc_buf = NULL;
        uint8_t *data_buf = NULL;
	uint8_t *enc_key = NULL;

	if (access(CONFIG_PATH, F_OK|R_OK) == -1)
		goto error;

	datafp = fopen(CONFIG_PATH, "r");
	if (datafp == NULL)
		goto error;

	fseek(datafp, 0, SEEK_END);
	file_size = ftell(datafp);
	rewind(datafp);

	enc_size = file_size - IV_SIZE;
	data_size = enc_size - TAG_SIZE;

	iv_buf = (char*)malloc(sizeof(char)*IV_SIZE);
	enc_buf = (char*)malloc(sizeof(char)*(enc_size));
	data_buf = (char*)malloc(sizeof(char)*(data_size));
	if (!iv_buf || !enc_buf || !data_buf)
		goto error;

	enc_key = get_key(PASSPHRASE);
	fread(iv_buf, sizeof(uint8_t), IV_SIZE, datafp);
	fread(enc_buf, sizeof(uint8_t), enc_size, datafp);

	if (aes_gcm_decrypt_core(data_buf, enc_buf, enc_size,
				 enc_key, iv_buf) != 0)
		goto error;

	FILE *tmp_file = tmpfile();
	if (tmp_file == NULL)
		goto error;
	fwrite(data_buf, sizeof(uint8_t), data_size,
		tmp_file);

	rewind(tmp_file);
	goto end;

error:
	tmp_file = NULL;
end:
	if (datafp)
		fclose(datafp);
	if (enc_key)
		free(enc_key);
	if (data_buf)
		free(data_buf);
	if (enc_buf)
		free(enc_buf);
	if (iv_buf)
		free(iv_buf);

	return tmp_file;
}

int32_t set_hcfs_config(char *arg_buf, uint32_t arg_len)
{

	int32_t idx, ret_code;
	uint32_t msg_len;
	ssize_t str_len;
	char tmp_path[100];
	char buf[300];
	char key[100], upper_key[100], value[400];
	char *tmp_ptr, *line, *token;
	FILE *conf = NULL;
	FILE *tmp_conf = NULL;
	uint8_t data_buf[1024];
	int32_t data_size = 0;


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

	if (msg_len != arg_len ||
	    _validate_config_key(key) < 0) {
		ret_code = -EINVAL;
		goto end;
	}

	memcpy(upper_key, key, sizeof(upper_key));
	for (idx = 0; idx < strlen(key); idx++)
		upper_key[idx] = toupper(upper_key[idx]);

	conf = _get_decrypt_configfp();
	if (conf == NULL) {
		goto error;
	}

	data_size = str_len = 0;
	while (fgets(buf, sizeof(buf), conf) != NULL) {
		tmp_ptr = line = strdup(buf);
		token = strsep(&line, " = ");
		if (strcmp(upper_key, token) == 0)
			snprintf(buf, sizeof(buf), "%s = %s\n", token, value);

		str_len = strlen(buf);
		memcpy(&(data_buf[data_size]), buf, str_len + 1);
		data_size += str_len;

		free(tmp_ptr);
	}

	uint8_t *enc_data =
		(uint8_t*)malloc(sizeof(char) * (data_size + IV_SIZE + TAG_SIZE));
	if (enc_data == NULL)
		goto error;

	ret_code = _encrypt_config(enc_data, data_buf, data_size);
	if (ret_code != 0) {
		ret_code = EIO;
		goto end;
	}

	snprintf(tmp_path, sizeof(tmp_path),"%s.%s",
		 CONFIG_PATH, "tmp");
	tmp_conf = fopen(tmp_path, "w");
	if (tmp_conf == NULL)
		goto error;

	fwrite(enc_data, sizeof(uint8_t),
	       data_size + IV_SIZE + TAG_SIZE, tmp_conf);

	ret_code = rename(tmp_path, CONFIG_PATH);
	if (ret_code < 0)
		goto error;
	else
		goto end;

error:
	ret_code = -errno;

end:
	if (conf)
		fclose(conf);
	if (tmp_conf)
		fclose(tmp_conf);
	return ret_code;
}

int32_t get_hcfs_config(char *arg_buf, uint32_t arg_len, char **value)
{

	int32_t idx, ret_code;
	uint32_t msg_len;
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

	conf = _get_decrypt_configfp();
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

int32_t reload_hcfs_config()
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = RELOADCONFIG;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

int32_t toggle_cloud_sync(char *arg_buf, uint32_t arg_len)
{

	int32_t fd, ret_code, size_msg, enabled;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;

	memcpy(&enabled, arg_buf, sizeof(int32_t));
	if (enabled != 0 && enabled != 1)
		return -EINVAL;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = SETSYNCSWITCH;
	cmd_len = sizeof(int32_t);

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);
	size_msg = send(fd, &enabled, sizeof(int32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

int32_t get_sync_status()
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETSYNCSWITCH;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

int32_t reset_xfer_usage()
{

	int32_t fd, ret_code, size_msg;
	uint32_t code, cmd_len, reply_len, total_recv, to_recv;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = RESETXFERSTAT;
	cmd_len = 0;

	size_msg = send(fd, &code, sizeof(uint32_t), 0);
	size_msg = send(fd, &cmd_len, sizeof(uint32_t), 0);

	size_msg = recv(fd, &reply_len, sizeof(uint32_t), 0);
	size_msg = recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/* Callback function for sql statement */
static int32_t _sqlite_exec_cb(void *data, int32_t argc, char **argv, char **azColName)
{

	size_t uid_len;
	char **uid = (char **)data;

	uid_len = argv[0] ? strlen(argv[0]) : strlen("NULL");
	*uid = malloc(sizeof(char) * (uid_len + 1));
	snprintf(*uid, uid_len + 1, "%s", argv[0]);

	return 0;
}

int32_t query_pkg_uid(char *arg_buf, uint32_t arg_len, char **uid)
{

	int32_t idx, ret_code;
	uint32_t msg_len;
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
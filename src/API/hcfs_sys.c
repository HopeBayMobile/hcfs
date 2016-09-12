/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfs_sys.c
* Abstract: This c source file for hcfs system operations.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#include "hcfs_sys.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sqlite3.h>
#include <inttypes.h>

#include "global.h"
#include "socket_util.h"
#include "enc.h"


/************************************************************************
 * *
 * * Function name: _validate_config_key
 * *        Inputs: char *key
 * *       Summary: To validate a given section (key) in config file.
 * *
 * *  Return value: 0 if successful. Otherwise returns -1 on errors.
 * *
 * *************************************************************************/
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

/************************************************************************
 * *
 * * Function name: _encrypt_config
 * *        Inputs: uint8_t *output, uint8_t *input
 * *		    int64_t input_len
 * *       Summary: To encrypt data (input) and store the encrypted content
 * *		    in (output). The size of *output* should be larger than
 * *    	    input_len + IV_SIZE + TAG_SIZE.
 * *
 * *  Return value: 0 if successful. Otherwise returns -1 on errors.
 * *
 * *************************************************************************/
int32_t _encrypt_config(uint8_t *output, uint8_t *input,
		        int64_t input_len)
{
	int32_t ret;
	uint8_t iv[IV_SIZE] = {0};
	uint8_t *enc_key, *enc_data;

	/* Key and iv */
	enc_key = get_key(PASSPHRASE);
	generate_random_bytes(iv, IV_SIZE);

	enc_data = (uint8_t*)malloc(
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

/************************************************************************
 * *
 * * Function name: _get_decrypt_configfp
 * *        Inputs: uint8_t *output, uint8_t *input
 * *		    int64_t input_len
 * *       Summary: To encrypt data (input) and store the encrypted content
 * *		    in (output). The size of *output* should be larger than
 * *    	    input_len + IV_SIZE + TAG_SIZE.
 * *
 * *  Return value: FILE pointer to decrypted file.
 * *
 * *************************************************************************/
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

	iv_buf = (uint8_t*)malloc(sizeof(char)*IV_SIZE);
	enc_buf = (uint8_t*)malloc(sizeof(char)*(enc_size));
	data_buf = (uint8_t*)malloc(sizeof(char)*(data_size));
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

/************************************************************************
 * *
 * * Function name: set_hcfs_config
 * *        Inputs: char *arg_buf, uint32_t arg_len
 * *       Summary: To set a section in config file with input value.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t set_hcfs_config(char *arg_buf, uint32_t arg_len)
{
	int32_t ret_code;
	uint32_t idx, msg_len;
	ssize_t str_len;
	char tmp_path[100];
	char buf[300];
	char key[100], upper_key[100], value[400];
	char *tmp_ptr, *line, *token;
	FILE *conf = NULL;
	FILE *tmp_conf = NULL;
	uint8_t *data_buf = NULL;
	uint8_t *enc_data = NULL;
	int32_t data_size = 0;
	int64_t data_buf_size;


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

	fseek(conf, 0, SEEK_END);
	data_buf_size = ftell(conf);
	rewind(conf);
	data_buf =
		(uint8_t*)malloc(sizeof(char) * (data_buf_size + arg_len));

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

	enc_data =
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
	if (data_buf)
		free(data_buf);
	if (enc_data)
		free(enc_data);
	return ret_code;
}

/************************************************************************
 * *
 * * Function name: get_hcfs_config
 * *        Inputs: char *arg_buf, uint32_t arg_len, char **value
 * *       Summary: Return the value of input section in config file.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_hcfs_config(char *arg_buf, uint32_t arg_len, char **value)
{
	int32_t ret_code;
	uint32_t idx, msg_len;
	ssize_t str_len;
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

/************************************************************************
 * *
 * * Function name: reload_hcfs_config
 * *        Inputs:
 * *       Summary: To notify hcfs to reload config.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t reload_hcfs_config()
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = RELOADCONFIG;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: toggle_cloud_sync
 * *        Inputs: char *arg_buf, uint32_t arg_len
 * *       Summary: To notify hcfs to turn on/off cloud synchronizing.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t toggle_cloud_sync(char *arg_buf, uint32_t arg_len)
{
	int32_t fd, ret_code, enabled;
	uint32_t code, cmd_len, reply_len;

	memcpy(&enabled, arg_buf, arg_len);
	if (enabled != 0 && enabled != 1)
		return -EINVAL;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = SETSYNCSWITCH;
	cmd_len = arg_len;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, &enabled, sizeof(int32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: get_sync_status
 * *        Inputs:
 * *       Summary: To get status of cloud synchronizing.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t get_sync_status()
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = GETSYNCSWITCH;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: reset_xfer_usage
 * *        Inputs:
 * *       Summary: To notify hcfs to reset the value of data transfer
 * *		    statistics.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t reset_xfer_usage()
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = RESETXFERSTAT;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: set_notify_server
 * *        Inputs: char *arg_buf, uint32_t arg_len
 * *       Summary: To set the location of event notify server.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t set_notify_server(char *arg_buf, uint32_t arg_len)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;
	char path[arg_len + 10];

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = SETNOTIFYSERVER;
	cmd_len = arg_len;

	memcpy(path, arg_buf, arg_len);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, path, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: set_swift_access_token
 * *        Inputs: char *arg_buf, uint32_t arg_len
 * *       Summary: To set the value of swift storage url and auth token.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code
 * *
 * *************************************************************************/
int32_t set_swift_access_token(char *arg_buf, uint32_t arg_len)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = SETSWIFTTOKEN;
	cmd_len = arg_len;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, arg_buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: toggle_sync_point
 * *        Inputs: int32_t api_code
 * *       Summary: To set/clear hcfs sync point. Parameter (api_code) should
 * *                be SETSYNCPOINT or CANCELSYNCPOINT.
 * *
 * *  Return value: 0 if successful.
 * *                1 if nothing changed.
 * *                Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t toggle_sync_point(int32_t api_code)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	if (api_code != SETSYNCPOINT && api_code != CANCELSYNCPOINT)
		return -EINVAL;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = api_code;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

<<<<<<< HEAD
/************************************************************************
 * *
 * * Function name: trigger_restore
 * *        Inputs:
 * *       Summary: To initiate a restoration process.
 * *
 * *  Return value: 0 if successful.
 * *                Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t trigger_restore()
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = INITIATE_RESTORATION;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: check_restore_status
 * *        Inputs:
 * *       Summary: To check the status of restoration process.
 * *
 * *  Return value: 0 if not being restored.
 * *		    1 if in stage 1 of restoration process.
 * *		    2 if in stage 2 of restoration process.
 * *                Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t check_restore_status()
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECK_RESTORATION_STATUS;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: notify_applist_change
 * *        Inputs:
 * *       Summary: To inform HCFS that package lists in packages.xml
 * *	            has changed and needs to be backed-up.
=======
/* helper function for system() result checking */
#define check_system_result()\
	do {\
		if (sys_ret == -1)\
			return -errno;\
		if (WIFSIGNALED(sys_ret))\
			return -WTERMSIG(sys_ret);\
		if (WEXITSTATUS(sys_ret) != 0)\
			return -WEXITSTATUS(sys_ret);\
	} while (0)

/************************************************************************
 * *
 * * Function name: collect_sys_logs
 * *        Inputs:
 * *       Summary: To copy/dump logs to "/sdcard/TeraLog/logs".
>>>>>>> android-dev
 * *
 * *  Return value: 0 if successful.
 * *                Otherwise returns negation of error code.
 * *
 * *************************************************************************/
<<<<<<< HEAD
int32_t notify_applist_change()
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = NOTIFY_APPLIST_CHANGE;
	cmd_len = 0;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
=======
int32_t collect_sys_logs()
{
	int32_t sys_ret, ret_code;
	int64_t hcfslog_size;
	char buf[4096];
	FILE *hcfslog_fptr = NULL, *new_hcfslog_fptr = NULL;
	struct stat tmpstat;

	char *log_dir = "/sdcard/TeraLog";
	char *hcfslog_path = "/data/hcfs_android_log";
	char *hcfslog_path2 = "/data/hcfs_android_log.1";
	char *cmd_cp_hcfslog = "cp /data/hcfs_android_log.1 /sdcard/TeraLog/.";
	char *cmd_dump_logcat = "logcat -d > /sdcard/TeraLog/logcat";
	char *cmd_dump_dmesg = "dmesg > /sdcard/TeraLog/dmesg";

	ret_code = stat(log_dir, &tmpstat);
	if (ret_code < 0) {
		if (errno == ENOENT)
			mkdir(log_dir, 0777);
		else
			return -errno;
	}

	ret_code = stat(hcfslog_path, &tmpstat);
	if (ret_code < 0)
		return -EIO;
	hcfslog_size = tmpstat.st_size;

	/* hcfslog */
	hcfslog_fptr = fopen(hcfslog_path, "r");
	if (hcfslog_fptr == NULL)
		return -errno;

	new_hcfslog_fptr = fopen("/sdcard/TeraLog/hcfs_android_log", "w");
	if (new_hcfslog_fptr == NULL) {
		fclose(hcfslog_fptr);
		return -errno;
	}

	while (fgets(buf, sizeof(buf), hcfslog_fptr) != NULL
			&& ftell(hcfslog_fptr) <= hcfslog_size) {
		if (fprintf(new_hcfslog_fptr, "%s", buf) < 0) {
			fclose(hcfslog_fptr);
			fclose(new_hcfslog_fptr);
			return -errno;
		}
	}

	fclose(hcfslog_fptr);
	fclose(new_hcfslog_fptr);

	if (access(hcfslog_path2, F_OK|R_OK) != -1) {
		sys_ret = system(cmd_cp_hcfslog);
		check_system_result();
	}

	/* logcat */
	sys_ret = system(cmd_dump_logcat);
	check_system_result();

	/* dmesg */
	sys_ret = system(cmd_dump_dmesg);
	check_system_result();

	return 0;
>>>>>>> android-dev
}

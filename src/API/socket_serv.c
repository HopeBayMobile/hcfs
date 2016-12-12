/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: socket_serv.c
* Abstract: This c source file for HCFSAPI socket server.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

#include "socket_serv.h"

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <libgen.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>

#include "global.h"
#include "hcfs_stat.h"
#include "hcfs_sys.h"
#include "logger.h"
#include "marco.h"
#include "minimal_apk.h"
#include "pin_ops.h"
#include "smart_cache.h"
#include "socket_util.h"

SOCK_THREAD thread_pool[MAX_THREAD];
sem_t thread_access_sem; /* For thread pool control */

int32_t do_pin_by_path(char *largebuf, int32_t arg_len,
		       char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start pin by path\n");
	ret_code = pin_by_path(largebuf, arg_len);

	if (ret_code > 0) ret_code = 0;

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End pin by path\n");
	return ret_code;
}

int32_t do_unpin_by_path(char *largebuf, int32_t arg_len,
			 char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start unpin by path\n");
	ret_code = unpin_by_path(largebuf, arg_len);

	if (ret_code > 0) ret_code = 0;

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End unpin by path\n");
	return ret_code;
}

int32_t do_check_dir_status(char *largebuf, int32_t arg_len,
			    char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;
	int64_t num_local, num_cloud, num_hybrid;

	write_log(8, "Start check dir stat\n");
	ret_code = check_dir_status(largebuf, arg_len,
				    &num_local, &num_cloud,
				    &num_hybrid);

	if (ret_code < 0) {
		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(&ret_code, sizeof(int32_t));
	} else {
		/* Total size for reply msgs */
		ret_len = sizeof(int64_t) * 3;

		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(&num_local, sizeof(int64_t));
		CONCAT_REPLY(&num_cloud, sizeof(int64_t));
		CONCAT_REPLY(&num_hybrid, sizeof(int64_t));
	}

	write_log(8, "End check dir stat\n");
	return ret_code;
}

int32_t do_check_file_loc(char *largebuf, int32_t arg_len,
			  char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start check file location\n");
	ret_code = check_file_loc(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End check file location\n");
	return ret_code;
}

int32_t do_check_pin_status(char *largebuf, int32_t arg_len,
			    char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start check pin status\n");
	ret_code = check_pin_status(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End check pin status\n");
	return ret_code;
}

int32_t do_set_hcfs_config(char *largebuf, int32_t arg_len,
			   char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start set config\n");
	ret_code = set_hcfs_config(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End set config\n");
	return ret_code;
}

int32_t do_get_hcfs_config(char *largebuf, int32_t arg_len,
			   char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;
	char *value;

	write_log(8, "Start get config\n");
	ret_code = get_hcfs_config(largebuf, arg_len, &value);

	if (ret_code == 0) {
		ret_len = strlen(value);
		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(value, ret_len);
		free(value);
	} else {
		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(&ret_code, sizeof(int32_t));
	}

	write_log(8, "End get config\n");
	return ret_code;
}

int32_t do_get_hcfs_stat(char *largebuf, int32_t arg_len,
			 char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;
	HCFS_STAT_TYPE stats;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start get statistics\n");
	ret_code = get_hcfs_stat(&stats);

	if (ret_code < 0) {
		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(&ret_code, sizeof(int32_t));
	} else {
		/* Total size for reply msgs */
		ret_len = sizeof(HCFS_STAT_TYPE);

		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(&stats, ret_len);
	}

	write_log(8, "End get statistics\n");
	return ret_code;
}

int32_t do_get_occupied_size(char *largebuf, int32_t arg_len,
			     char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;
	int64_t occupied;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start get occupied size\n");
	ret_code = get_occupied_size(&occupied);

	if (ret_code < 0) {
		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(&ret_code, sizeof(int32_t));
	} else {
		/* Total size for reply msgs */
		ret_len = sizeof(int64_t) + sizeof(int32_t);

		CONCAT_REPLY(&ret_len, sizeof(uint32_t));
		CONCAT_REPLY(&occupied, sizeof(int64_t));
	}

	write_log(8, "End get occupied size\n");
	return ret_code;
}

int32_t do_reset_xfer_usage(char *largebuf, int32_t arg_len,
			    char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start reset xfer stats\n");
	ret_code = reset_xfer_usage(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End reset xfer stats\n");
	return ret_code;
}

int32_t do_toggle_cloud_sync(char *largebuf, int32_t arg_len,
			     char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start toggle sync\n");
	ret_code = toggle_cloud_sync(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End toggle sync\n");
	return ret_code;
}

int32_t do_get_sync_status(char *largebuf, int32_t arg_len,
			   char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start get sync status\n");
	ret_code = get_sync_status(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End get sync status\n");
	return ret_code;
}

int32_t do_reload_hcfs_config(char *largebuf, int32_t arg_len,
			      char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start reload config\n");
	ret_code = reload_hcfs_config(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End reload config\n");
	return ret_code;
}

int32_t do_set_notify_server(char *largebuf, int32_t arg_len,
			     char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start set event notify server\n");
	ret_code = set_notify_server(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End set event notify server\n");
	return ret_code;
}

int32_t do_set_swift_token(char *largebuf, int32_t arg_len,
			   char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	write_log(8, "Start set swift access token\n");
	ret_code = set_swift_access_token(largebuf, arg_len);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End set swift access token\n");
	return ret_code;
}

int32_t do_set_sync_point(char *largebuf, int32_t arg_len,
			  char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start set sync point\n");
	ret_code = toggle_sync_point(SETSYNCPOINT);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End set sync point\n");
	return ret_code;
}

int32_t do_clear_sync_point(char *largebuf, int32_t arg_len,
			    char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start clear sync point\n");
	ret_code = toggle_sync_point(CANCELSYNCPOINT);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End clear sync point\n");
	return ret_code;
}

int32_t do_collect_sys_logs(char *largebuf, int32_t arg_len,
			    char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start collect sys logs\n");
	ret_code = collect_sys_logs();

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End collect sys logs\n");
	return ret_code;
}

int32_t do_trigger_restore(char *largebuf, int32_t arg_len,
			   char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start trigger restore\n");
	ret_code = trigger_restore();

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End trigger restore\n");
	return ret_code;
}

int32_t do_check_restore_status(char *largebuf, int32_t arg_len,
				char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start check restore status\n");
	ret_code = check_restore_status();

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End check restore status\n");
	return ret_code;
}

int32_t do_notify_applist_change(char *largebuf, int32_t arg_len,
				 char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Start notify applist change\n");
	ret_code = notify_applist_change();

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End notify applist change\n");
	return ret_code;
}

int32_t do_check_package_boost_status(char *largebuf, int32_t arg_len,
			   	      char *resbuf, int32_t *res_size)
{
	char package_name[arg_len + 10];
	int32_t ret_code;
	uint32_t ret_len = 0;
	int64_t str_len = 0;

	write_log(8, "Check package boost status\n");

	memcpy(&str_len, largebuf, sizeof(int64_t));
	strncpy(package_name, largebuf + sizeof(int64_t), str_len);
	ret_code = check_pkg_boost_status(package_name);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End check package boost status\n");
	return ret_code;
}

int32_t do_clear_boosted_package(char *largebuf, int32_t arg_len,
				 char *resbuf, int32_t *res_size)
{
	char package_name[arg_len + 10];
	int32_t ret_code;
	uint32_t ret_len = 0;
	int64_t str_len = 0;

	write_log(8, "Clear boosted package\n");

	memcpy(&str_len, largebuf, sizeof(int64_t));
	strncpy(package_name, largebuf + sizeof(int64_t), str_len);
	ret_code = clear_boosted_package(package_name);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End clear boosted package\n");
	return ret_code;
}

int32_t do_enable_booster(char *largebuf, int32_t arg_len,
			  char *resbuf, int32_t *res_size)
{
	int32_t ret_code;
	uint32_t ret_len = 0;
	int64_t smart_cache_size;

	UNUSED(arg_len);

	write_log(8, "Enable booster\n");

	memcpy(&smart_cache_size, largebuf, sizeof(int64_t));
	if (smart_cache_size < 1048576) /* At least 1MB */
		return -EINVAL;

	ret_code = enable_booster(smart_cache_size);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End enable booster\n");
	return ret_code;
}

int32_t do_trigger_boost(char *largebuf, int32_t arg_len,
			 char *resbuf, int32_t *res_size,
			 pthread_t *tid)
{
	int32_t ret_code = 0;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Trigger boost\n");

	ret_code = trigger_boost(TOBOOST, tid);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End Trigger boost\n");
	return ret_code;
}

int32_t do_trigger_unboost(char *largebuf, int32_t arg_len,
			   char *resbuf, int32_t *res_size,
			   pthread_t *tid)
{
	int32_t ret_code = 0;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Trigger unboost\n");

	ret_code = trigger_boost(TOUNBOOST, tid);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End Trigger unboost\n");
	return ret_code;
}

int32_t do_mount_smart_cache(char *largebuf, int32_t arg_len,
			     char *resbuf, int32_t *res_size)
{
	char to_mount = TRUE;
	int32_t ret_code = 0;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Mount smart cache\n");

	ret_code = toggle_smart_cache_mount(to_mount);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End mount smart cache\n");
	return ret_code;
}

int32_t do_umount_smart_cache(char *largebuf, int32_t arg_len,
			      char *resbuf, int32_t *res_size)
{
	char to_mount = FALSE;
	int32_t ret_code = 0;
	uint32_t ret_len = 0;

	UNUSED(largebuf);
	UNUSED(arg_len);

	write_log(8, "Umount smart cache\n");

	ret_code = toggle_smart_cache_mount(to_mount);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End umount smart cache\n");
	return ret_code;
}

int32_t do_check_minimal_apk(char *largebuf, int32_t arg_len,
			     char *resbuf, int32_t *res_size)
{
	char package_name[arg_len + 10];
	int32_t ret_code;
	uint32_t ret_len = 0;
	int64_t str_len = 0;

	write_log(8, "Check minimal apk\n");

	memcpy(&str_len, largebuf, sizeof(int64_t));
	strncpy(package_name, largebuf + sizeof(int64_t), str_len);
	ret_code = check_minimal_apk(package_name);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End check minimal apk\n");
	return ret_code;
}

int32_t do_create_minimal_apk(char *largebuf, int32_t arg_len,
			      char *resbuf, int32_t *res_size)
{
	char package_name[arg_len + 10];
	int32_t ret_code;
	uint32_t ret_len = 0;
	int64_t str_len = 0;

	write_log(8, "Create minimal apk\n");

	memcpy(&str_len, largebuf, sizeof(int64_t));
	strncpy(package_name, largebuf + sizeof(int64_t), str_len);
	ret_code = create_minimal_apk(package_name);

	CONCAT_REPLY(&ret_len, sizeof(uint32_t));
	CONCAT_REPLY(&ret_code, sizeof(int32_t));

	write_log(8, "End create minimal apk\n");
	return ret_code;
}

/************************************************************************
 * *
 * * Function name: _get_unused_thread
 * *        Inputs:
 * *       Summary: To find an unused thread.
 * *
 * *  Return value: thread id if successful.
 * *		    Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t _get_unused_thread()
{
	int32_t idx;

	sem_wait(&thread_access_sem);
	for (idx = 0; idx < MAX_THREAD; idx++) {
		if (!thread_pool[idx].in_used) {
			thread_pool[idx].in_used = TRUE;
			sem_post(&thread_access_sem);
			return idx;
		}
	}
	sem_post(&thread_access_sem);
	return -1;
}

/************************************************************************
 * *
 * * Function name: process_request
 * *        Inputs: int32_t thread_idx
 * *       Summary: To process an API request by creating a new
 * *		    thread (thread_idx).
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t process_request(void *arg)
{
	int32_t thread_idx, fd, ret_code, res_size;
	uint32_t api_code, arg_len;
	char buf_reused;
	char buf[512], resbuf[512];
	char *largebuf;

	thread_idx = (intptr_t)arg;

	fd = thread_pool[thread_idx].fd;
	write_log(8, "Process a new request with socket fd %d and thread_id %d",
			fd, thread_idx);

	if (reads(fd, &api_code, sizeof(uint32_t))) {
		write_log(0, "[%d] Failed to receive API code\n", thread_idx);
		goto error;
	}
	write_log(8, "[%d] API code is %d\n", thread_idx, api_code);

	if (reads(fd, &arg_len, sizeof(uint32_t))) {
		write_log(0, "Failed to receive arg length. (API code %d)\n",
				api_code);
		goto error;
	}
	write_log(8, "[%d] Argument length is %d\n", thread_idx, arg_len);

	if (arg_len < 500) {
		largebuf = buf;
		buf_reused = TRUE;
	} else {
		largebuf = malloc(arg_len + 20);
		if (largebuf == NULL)
			goto error;
	}

	if (reads(fd, largebuf, arg_len)) {
		write_log(0, "[%d] Failed to receive args. (API code %d)\n",
				thread_idx, api_code);
		goto error;
	}

	SOCK_CMDS cmds[] = {
		{PIN,				do_pin_by_path},
		{UNPIN,				do_unpin_by_path},
		{CHECKDIRSTAT,			do_check_dir_status},
		{CHECKLOC,			do_check_file_loc},
		{CHECKPIN,			do_check_pin_status},
		{SETCONFIG,			do_set_hcfs_config},
		{GETCONFIG,			do_get_hcfs_config},
		{GETSTAT,			do_get_hcfs_stat},
		{RESETXFERSTAT,			do_reset_xfer_usage},
		{SETSYNCSWITCH,			do_toggle_cloud_sync},
		{GETSYNCSWITCH,			do_get_sync_status},
		{RELOADCONFIG,			do_reload_hcfs_config},
		{OCCUPIEDSIZE,			do_get_occupied_size},
		{SETNOTIFYSERVER,		do_set_notify_server},
		{SETSWIFTTOKEN,			do_set_swift_token},
		{SETSYNCPOINT,			do_set_sync_point},
		{CANCELSYNCPOINT,		do_clear_sync_point},
		{INITIATE_RESTORATION,		do_trigger_restore},
		{CHECK_RESTORATION_STATUS,	do_check_restore_status},
		{NOTIFY_APPLIST_CHANGE,		do_notify_applist_change},
		{COLLECTSYSLOGS,		do_collect_sys_logs},
		{CHECK_PACKAGE_BOOST_STATUS,	do_check_package_boost_status},
		{ENABLE_BOOSTER,		do_enable_booster},
		{CLEAR_BOOSTED_PACKAGE,		do_clear_boosted_package},
		{MOUNT_SMART_CACHE,		do_mount_smart_cache},
		{UMOUNT_SMART_CACHE,		do_umount_smart_cache},
		{CHECK_MINI_APK,		do_check_minimal_apk},
		{CREATE_MINI_APK,		do_create_minimal_apk},
	};

	/* Asynchronous API will return immediately and process cmd in
	 * background */
	SOCK_ASYNC_CMDS async_cmds[] = {
		{TRIGGER_BOOST,		do_trigger_boost},
		{TRIGGER_UNBOOST,	do_trigger_unboost},
	};

	uint32_t n;
	for (n = 0; n < sizeof(cmds) / sizeof(cmds[0]); n++) {
		if (api_code == cmds[n].name) {
			res_size = 0;
			ret_code = cmds[n].cmd_fn(largebuf, arg_len, resbuf,
						  &res_size);
			sends(fd, resbuf, res_size);
			goto done;
		}
	}
	/* async APIs */
	for (n = 0; n < sizeof(async_cmds) / sizeof(async_cmds[0]); n++) {
		if (api_code == async_cmds[n].name) {
			write_log(8, "Start asynchronous API (%d)", api_code);
			res_size = 0;
			ret_code = async_cmds[n].cmd_fn(
			    largebuf, arg_len, resbuf, &res_size,
			    &(async_cmds[n].async_thread));
			sends(fd, resbuf, res_size);

			/* Wait for async thread terminated */
			pthread_join(async_cmds[n].async_thread, NULL);
			write_log(8, "Asynchronous API (%d) finished",
				  api_code);
			goto done;
		}
	}
	write_log(0, "[%d] API code not found (API code %d)\n",
			thread_idx, api_code);

	ret_code = -EINVAL;
	res_size = 0;
	sends(fd, &res_size, sizeof(int32_t));
	sends(fd, &ret_code, sizeof(int32_t));
	goto done;

error:
	ret_code = -EAGAIN;
	res_size = 0;
	sends(fd, &res_size, sizeof(int32_t));
	sends(fd, &ret_code, sizeof(int32_t));

done:
	write_log(8, "[%d] Finished API request (API code %d)\n",
			thread_idx, api_code);
	close(fd);
	if (largebuf != NULL && !buf_reused)
		free(largebuf);

	sem_wait(&thread_access_sem);
	thread_pool[thread_idx].in_used = FALSE;
	sem_post(&thread_access_sem);

	return ret_code;
}

/************************************************************************
 * *
 * * Function name: init_server
 * *        Inputs:
 * *       Summary: To initialize the HCFSAPI socket server.
 * *
 * *  Return value: 0 if successful. Otherwise returns negation of error code.
 * *
 * *************************************************************************/
int32_t init_server()
{
	int32_t sock_fd, sock_flag;
	int32_t new_sock_fd, new_thread_idx, ret_code, count;
	char sock_path[200];
	char *sock_dir;
	struct sockaddr_un addr;
	struct timespec timer;


	for (count = 0; count < MAX_THREAD; count++) {
		thread_pool[count].fd = 0;
		thread_pool[count].in_used = FALSE;
	}

	strcpy(sock_path, API_SOCK_PATH);

	/* To wait api socket path ready */
	sock_dir = dirname(sock_path);
	while (1) {
		if (access(sock_dir, F_OK | W_OK) == 0) {
			break;
		} else {
			write_log(0, "Waiting socket path ready...\n");
			sleep(1);
		}
	}

	if (access(API_SOCK_PATH, F_OK) == 0)
		unlink(API_SOCK_PATH);

	addr.sun_family = AF_UNIX;
	strcpy(addr.sun_path, API_SOCK_PATH);

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		write_log(0, "Failed to get socket fd. Code %d, %s\n",
				errno, strerror(errno));
		return -errno;
	}

	if (bind(sock_fd, (struct sockaddr*) &addr,
	    sizeof(struct sockaddr_un)) < 0) {
		write_log(0, "Failed to assign name to socket. Code %d, %s\n",
				errno, strerror(errno));
		return -errno;
	}

	sock_flag = fcntl(sock_fd, F_GETFL, 0);
	sock_flag = sock_flag | O_NONBLOCK;
	fcntl(sock_fd, F_SETFL, sock_flag);

	ret_code = listen(sock_fd, 16);
	if (ret_code < 0) {
		write_log(0, "Failed to listen socket. Code %d, %s\n",
				errno, strerror(errno));
		return -errno;
	}

	write_log(0, "HCFSAPID is ready.\n");

	timer.tv_sec = 0;
	timer.tv_nsec = 100000000;

	while (1) {
		new_sock_fd = accept(sock_fd, NULL, NULL);
		if (new_sock_fd < 0) {
			nanosleep(&timer, NULL);
			continue;
		}

		while (1) {
			new_thread_idx = _get_unused_thread();
			if (new_thread_idx < 0) {
				sem_post(&thread_access_sem);
				nanosleep(&timer, NULL);
				continue;
			} else {
				thread_pool[new_thread_idx].fd = new_sock_fd;
				/* Set thread detached */
				pthread_attr_init(&(thread_pool[new_thread_idx].attr));
				pthread_attr_setdetachstate(&(thread_pool[new_thread_idx].attr),
					    PTHREAD_CREATE_DETACHED);
				pthread_create(&(thread_pool[new_thread_idx].thread),
					       &(thread_pool[new_thread_idx].attr),
					       (void *)process_request, (void*)(intptr_t)new_thread_idx);
				break;
			}
		}
	}

	close(sock_fd);
	return 0;
}

int32_t main()
{
	open_log(LOG_NAME);
	write_log(0, "Initailizing...");
	sem_post(&thread_access_sem);
	memset(thread_pool, 0, sizeof(SOCK_THREAD) * MAX_THREAD);
	sem_init(&thread_access_sem, 0, 1);
	write_log(0, "Starting HCFSAPID Server...");
	init_server();
	write_log(0, "End HCFSAPID Server");
	return 0;
}

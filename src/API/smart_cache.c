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

#include "smart_cache.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sqlite3.h>

#include "global.h"
#include "socket_util.h"
#include "marco.h"
#include "logger.h"

/* Helper function to create hcfs volume
 *
 * @return Return code returned by hcfs api service.
 */
int32_t _create_hcfs_vol()
{
	char buf[strlen(SMARTCACHEVOL) + 2];
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CREATEVOL;
	cmd_len = strlen(SMARTCACHEVOL) + 2;
	strncpy(buf, SMARTCACHEVOL, sizeof(buf));
	buf[cmd_len - 1] = ANDROID_INTERNAL;

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/* Helper function to mount smart cache volume
 *
 * @return Return code returned by hcfs api service.
 */
int32_t _mount_smart_cache_vol()
{
	char buf[1024];
	int32_t fd, ret_code, fsname_len, first_size, rest_size;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = MOUNTVOL;

	buf[0] = MP_DEFAULT;
	cmd_len = strlen(SMARTCACHEVOL) + strlen(SMARTCACHE) + 2 +
		  sizeof(int32_t) + sizeof(char);
	fsname_len = strlen(SMARTCACHEVOL) + 1;
	memcpy(buf + 1, &fsname_len, sizeof(int32_t));
	first_size = sizeof(int32_t) + 1;
	rest_size = sizeof(buf) - first_size;

	/* [mp mode][fsname len][fsname][mp] */
	snprintf(&(buf[first_size]), rest_size, "%s", SMARTCACHEVOL);
	snprintf(&(buf[first_size + fsname_len]),
		 sizeof(buf) - fsname_len - sizeof(int32_t) - sizeof(char),
		 "%s", SMARTCACHE);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/* Helper function to umount smart cache volume
 *
 * @return Return code returned by hcfs api service.
 */
int32_t _umount_smart_cache_vol()
{
	char buf[1024];
	int32_t fd, ret_code, fsname_len;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = UNMOUNTVOL;
	cmd_len =
	    sizeof(int32_t) + strlen(SMARTCACHEVOL) + strlen(SMARTCACHE) + 2;

	fsname_len = strlen(SMARTCACHEVOL);
	memcpy(buf, &fsname_len, sizeof(int32_t));
	strcpy(buf + sizeof(int32_t), SMARTCACHEVOL);
	strcpy(buf + sizeof(int32_t) + fsname_len + 1, SMARTCACHE);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/* Helper function to check mount status of  smart cache volume
 *
 * @return Return code returned by hcfs api service.
 */
int32_t _check_smart_cache_vol_mount()
{
	char buf[1024];
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = CHECKMOUNT;
	cmd_len = strlen(SMARTCACHEVOL) + 1;
	strncpy(buf, SMARTCACHEVOL, sizeof(buf));

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/* Helper function to delete smart cache volume
 *
 * @return Return code returned by hcfs api service.
 */
int32_t _delete_smart_cache_vol()
{
	char buf[1024];
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = DELETEVOL;
	cmd_len = strlen(SMARTCACHEVOL) + 1;
	strncpy(buf, SMARTCACHEVOL, sizeof(buf));

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, buf, cmd_len, 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/* Helper function to send event to notify server
 *
 * @return Return code returned by hcfs api service.
 */
int32_t _send_notify_event(int32_t event_id)
{
	int32_t fd, ret_code;
	uint32_t code, cmd_len, reply_len;

	fd = get_hcfs_socket_conn();
	if (fd < 0)
		return fd;

	code = SEND_NOTIFY_EVENT;
	cmd_len = sizeof(int32_t);

	send(fd, &code, sizeof(uint32_t), 0);
	send(fd, &cmd_len, sizeof(uint32_t), 0);
	send(fd, &event_id, sizeof(int32_t), 0);

	recv(fd, &reply_len, sizeof(uint32_t), 0);
	recv(fd, &ret_code, sizeof(int32_t), 0);

	close(fd);

	return ret_code;
}

/* Helper function to remove nonempty folder by system cmd.
 */
int32_t _remove_folder(char *pathname)
{
	char cmd[1024];
	char cmd_remove_folder[] = "rm -rf %s";
	int32_t status;

	snprintf(cmd, sizeof(cmd), cmd_remove_folder, pathname);
	status = system(cmd);
	if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
		write_log(0, "In %s. Failed to run cmd %s", __func__, cmd);
		return -1;
	}
	return 0;
}

/* To setup environment of smart cache. This API will create image file, setup
 * loop device, process ext4 mkfs and mount.
 *
 * @return 0 if successful. Otherwise returns negation of error code.
 */
int32_t enable_booster(int64_t smart_cache_size)
{
	char cmd[1024];
	char cmd_create_image_file[] = "dd if=/dev/zero of=%s bs=1048576 count=%d";
	char cmd_setup_loop_device[] = "losetup %s %s";
	char cmd_create_ext4_fs[] = "make_ext4fs %s";
	char cmd_mount_ext4_fs[] = "mount -t ext4 %s %s";
	char cmd_restorecon[] = "restorecon %s";
	char image_file_path[strlen(SMARTCACHE) + strlen(HCFSBLOCK) + 10];
	int32_t ret_code, status;

	snprintf(image_file_path, sizeof(image_file_path), "%s/%s", SMARTCACHE,
		 HCFSBLOCK);

	ret_code = _create_hcfs_vol();
	if (ret_code < 0 && ret_code != -EEXIST) {
		write_log(0,
			  "In %s. Failed to create volume %s. Error code - %d",
			  __func__, SMARTCACHEVOL, ret_code);
		return ret_code;
	}

	rmdir(SMARTCACHE);
	ret_code = mkdir(SMARTCACHE, 0771);
	if (ret_code < 0 && errno != EEXIST) {
		write_log(0, "In %s. Failed to mkdir %s. Error code - %d",
			  __func__, SMARTCACHE, errno);
		return ret_code;
	}
	/* chown to system:sytem */
	chown(SMARTCACHE, 1000, 1000);
	RUN_CMD_N_CHECK(cmd_restorecon, SMARTCACHE);

	mkdir(SMARTCACHEAMNT, 0771);
	RUN_CMD_N_CHECK(cmd_restorecon, SMARTCACHEAMNT);

	rmdir(SMARTCACHEMTP);
	ret_code = mkdir(SMARTCACHEMTP, 0771);
	if (ret_code < 0 && errno != EEXIST) {
		write_log(0, "In %s. Failed to mkdir %s. Error code - %d",
			  __func__, SMARTCACHEMTP, errno);
		return ret_code;
	}
	RUN_CMD_N_CHECK(cmd_restorecon, SMARTCACHEMTP);

	if (_check_smart_cache_vol_mount() != 0) {
		ret_code = _mount_smart_cache_vol();
		if (ret_code < 0) {
			write_log(
			    0,
			    "In %s. Failed to mount volume %s. Error code - %d",
			    __func__, SMARTCACHEVOL, ret_code);
			return ret_code;
		}
		RUN_CMD_N_CHECK(cmd_restorecon, SMARTCACHE);
	}

	RUN_CMD_N_CHECK(cmd_create_image_file, image_file_path,
			(smart_cache_size / 1048576));
	RUN_CMD_N_CHECK(cmd_restorecon, image_file_path);
	RUN_CMD_N_CHECK(cmd_setup_loop_device, LOOPDEV, image_file_path);
	RUN_CMD_N_CHECK(cmd_create_ext4_fs, LOOPDEV);
	RUN_CMD_N_CHECK(cmd_mount_ext4_fs, LOOPDEV, SMARTCACHEMTP);
	RUN_CMD_N_CHECK(cmd_restorecon, SMARTCACHEMTP);

	return 0;

rollback:
	if (access(image_file_path, F_OK) != -1)
		_remove_folder(image_file_path);
	_umount_smart_cache_vol();
	_delete_smart_cache_vol();

	return ret_code;
}

/* To change boost status of package in database.
 */
int32_t change_boost_status(char *pkg_name, int32_t status)
{
	int32_t ret_code;
	char *sql_err = 0;
	char sql[1024];
	sqlite3 *db;

	ret_code = sqlite3_open(SMARTCACHE_DB_PATH, &db);
	if (ret_code != SQLITE_OK) {
		return -1;
	}

	snprintf(sql, sizeof(sql),
		 "Update %s SET boost_status=%d WHERE package_name='%s'",
		 SMARTCACHE_TABLE_NAME, status, pkg_name);

	ret_code = sqlite3_exec(db, sql, NULL, NULL, &sql_err);
	if (ret_code != SQLITE_OK) {
		write_log(0, "In %s. Error msg - %s\n", __func__, sql_err);
		sqlite3_free(sql_err);
		sqlite3_close(db);
		return -1;
	}

	sqlite3_close(db);
	return 0;
}

/* To move package data - Move data from /data/data/<pkg_folder> to smart cache.
 *
 * @return Return 0 if success. Otherwise, return -1.
 */
int32_t boost_package(char *package_name)
{
	char pkg_fullpath[strlen(package_name) + strlen(DATA_PREFIX) + 10];
	char pkg_tmppath[strlen(package_name) + strlen(DATA_PREFIX) + 20];
	char smart_cache_fullpath[strlen(SMARTCACHEMTP) + strlen(package_name) +
				  10];
	char cmd[1024];
	char cmd_copy_pkg_data[] = "cp -rp %s %s";
	char cmd_restorecon_recursive[] = "restorecon -R %s";
	int32_t ret_code, status;

	CHANGE_PKG_BOOST_STATUS(package_name, ST_BOOSTING);

	CHANGE_PKG_BOOST_STATUS(package_name, ST_BOOSTING);

	snprintf(pkg_fullpath, sizeof(pkg_fullpath), "%s/%s", DATA_PREFIX,
		 package_name);
	snprintf(smart_cache_fullpath, sizeof(smart_cache_fullpath), "%s/%s",
		 SMARTCACHEMTP, package_name);
	snprintf(pkg_tmppath, sizeof(pkg_tmppath), "%s.tmp", pkg_fullpath);

	if (access(pkg_fullpath, F_OK) == -1) {
		write_log(0, "In %s. Pacakge path %s not existed.", __func__,
			  pkg_fullpath);
		return -1;
	}

	if (access(SMARTCACHEMTP, F_OK) == -1) {
		write_log(0, "In %s. Smart cache not existed.", __func__);
		return -1;
	}

	/* If pkg_tmppath existed means that previous boosting for this package
	 * had been interrupted. Just finish the unfinished parts */
	if (access(pkg_tmppath, F_OK) == -1) {
		REMOVE_IF_EXIST(smart_cache_fullpath);
		RUN_CMD_N_CHECK(cmd_copy_pkg_data, pkg_fullpath, SMARTCACHEMTP);

		ret_code = rename(pkg_fullpath, pkg_tmppath);
		if (ret_code < 0) {
			write_log(0, "In %s. Failed to create temp file %s. "
				     "Error code %d",
				  __func__, pkg_tmppath, errno);
			goto rollback;
		}
	}
	RUN_CMD_N_CHECK(cmd_restorecon_recursive, smart_cache_fullpath);

	REMOVE_IF_EXIST(pkg_fullpath);

	ret_code = symlink(smart_cache_fullpath, pkg_fullpath);
	if (ret_code < 0 && errno != EEXIST) {
		write_log(0, "In %s. Failed to create link %s. Error code %d",
			  __func__, smart_cache_fullpath, errno);
		goto rollback;
	}

	_remove_folder(pkg_tmppath);

	CHANGE_PKG_BOOST_STATUS(package_name, ST_BOOSTED);
	ret_code = 0;
	goto end;

rollback:
	if (access(smart_cache_fullpath, F_OK) != -1)
		_remove_folder(smart_cache_fullpath);

	if (access(pkg_tmppath, F_OK) != -1) {
		unlink(pkg_fullpath);
		rename(pkg_tmppath, pkg_fullpath);
	}

	CHANGE_PKG_BOOST_STATUS(package_name, ST_BOOST_FAILED);
	ret_code = -1;

end:
	return ret_code;
}

/* To move package data - Move data from smart cache to /data/data/<pkg_folder>.
 *
 * @return Return 0 if success. Otherwise, return -1.
 */
int32_t unboost_package(char *package_name)
{
	char pkg_fullpath[strlen(package_name) + strlen(DATA_PREFIX) + 10];
	char pkg_tmppath[strlen(package_name) + strlen(DATA_PREFIX) + 20];
	char smart_cache_fullpath[strlen(SMARTCACHEMTP) + strlen(package_name) +
				  10];
	char cmd[1024];
	char cmd_copy_pkg_data[] = "cp -rp %s %s";
	char cmd_restorecon_recursive[] = "restorecon -R %s";
	int32_t ret_code, status;
	struct stat tmp_st;

	CHANGE_PKG_BOOST_STATUS(package_name, ST_UNBOOSTING);

	snprintf(pkg_fullpath, sizeof(pkg_fullpath), "%s/%s", DATA_PREFIX,
		 package_name);
	snprintf(smart_cache_fullpath, sizeof(smart_cache_fullpath), "%s/%s",
		 SMARTCACHEMTP, package_name);
	snprintf(pkg_tmppath, sizeof(pkg_tmppath), "%s.tmp", pkg_fullpath);

	if (access(SMARTCACHEMTP, F_OK) == -1) {
		write_log(0, "In %s. Smart cache not existed.", __func__);
		return -1;
	}

	/* If the type of pkg_fullpath isn't symlnk means that previous
	 * unboosting for this package had been interrupted. Just finish the
	 * unfinished parts */
	ret_code = lstat(pkg_fullpath, &tmp_st);
	if (ret_code < 0 || S_ISLNK(tmp_st.st_mode)) {
		REMOVE_IF_EXIST(pkg_fullpath);
		REMOVE_IF_EXIST(pkg_tmppath);

		RUN_CMD_N_CHECK(cmd_copy_pkg_data, smart_cache_fullpath,
				pkg_tmppath);

		ret_code = rename(pkg_tmppath, pkg_fullpath);
		if (ret_code < 0) {
			write_log(0, "In %s. Failed to create temp file %s. "
				     "Error code %d",
				  __func__, pkg_tmppath, errno);
			goto rollback;
		}
	}

	RUN_CMD_N_CHECK(cmd_restorecon_recursive, pkg_fullpath);

	_remove_folder(smart_cache_fullpath);

	CHANGE_PKG_BOOST_STATUS(package_name, ST_UNBOOSTED);
	ret_code = 0;
	goto end;

rollback:
	if (access(pkg_fullpath, F_OK) != -1) {
		_remove_folder(pkg_fullpath);
	}
	if (access(pkg_tmppath, F_OK) != -1) {
		_remove_folder(pkg_tmppath);
	}
	symlink(smart_cache_fullpath, pkg_fullpath);

	CHANGE_PKG_BOOST_STATUS(package_name, ST_UNBOOST_FAILED);
	ret_code = -1;

end:
	return ret_code;
}

/* Sqlite callback function to iterate packages to boost/unboost.
 */
static int32_t _iterate_pkg_cb(void *data,
			       int32_t argc,
			       char **argv,
			       char **azColName)
{
	char *package_name;
	char **tmp_ptr;
	int32_t num_pkg;
	BOOST_JOB_META *boost_job;

	UNUSED(argc);
	UNUSED(azColName);

	if (!argv[0]) {
		write_log(0, "In %s. No matched result.", __func__);
		return -1;
	}

	if (strncmp(argv[0], "", 1) == 0 || (strncmp(argv[0], " ", 1) == 0)) {
		write_log(0, "In %s. Invalid package name.", __func__);
		return -1;
	}

	package_name = argv[0];

	boost_job = (BOOST_JOB_META *)data;

	num_pkg = boost_job->num_pkg;

	/* Check if need to expand pkg list */
	if (num_pkg >= boost_job->pkg_list_size) {
		write_log(4, "In %s. To expand size of pkg list. (num_pkg %d, "
			     "pkg_list_size %d)",
			  __func__, num_pkg, boost_job->pkg_list_size);

		tmp_ptr = (char **)realloc(boost_job->pkg_list,
					   boost_job->pkg_list_size * 2 *
					       sizeof(char **));
		if (tmp_ptr == NULL) {
			write_log(4, "In %s. Failed to expand pkg list.",
				  __func__);
			return -1;
		}
		boost_job->pkg_list = tmp_ptr;
		boost_job->pkg_list_size *= 2;
	}

	/* Add pkg to list */
	boost_job->pkg_list[num_pkg] = strdup(package_name);
	if (boost_job->pkg_list[num_pkg] == NULL) {
		write_log(0, "In %s, Failed to alloc memory.", __func__);
		return -1;
	}
	boost_job->num_pkg += 1;

	return 0;
}

/* To query database for selected pacakge list and process packages
 * boosting/unboosting.
 */
void *process_boost(void *ptr)
{
	BOOL boost_fail = FALSE;
	char *package_name;
	char *sql_err = 0;
	char sql[1024];
	int32_t ret_code, retry_times, idx;
	BOOST_JOB_META *boost_job = NULL;
	sqlite3 *db;

	boost_job = (BOOST_JOB_META *)calloc(1, sizeof(BOOST_JOB_META));
	if (boost_job == NULL) {
		write_log(0,
			  "Failed to alloc memory for boost meta. Error - %d.",
			  errno);
		pthread_exit(&(int){ -1 });
	}

	boost_job->to_boost = *(int *)ptr;
	free(ptr);

	write_log(4, "Start to process packages %s",
		  (boost_job->to_boost) ? "boosting" : "unboosting");

	boost_job->num_pkg = 0;
	boost_job->pkg_list_size = DEFAULT_PKG_LIST_SIZE;
	boost_job->pkg_list = (char **)calloc(1, boost_job->pkg_list_size);
	if (boost_job->pkg_list == NULL) {
		write_log(0, "In %s. Faield to alloc memory for pkg list.",
			  __func__);
		boost_fail = TRUE;
		goto cleanup;
	}

	/* Find target packages */
	if (boost_job->to_boost)
		snprintf(sql, sizeof(sql),
			 "SELECT package_name from %s WHERE boost_status=%d",
			 SMARTCACHE_TABLE_NAME, ST_INIT_BOOST);
	else
		snprintf(sql, sizeof(sql),
			 "SELECT package_name from %s WHERE boost_status=%d",
			 SMARTCACHE_TABLE_NAME, ST_INIT_UNBOOST);

	if (access(SMARTCACHE_DB_PATH, F_OK) != 0) {
		boost_fail = TRUE;
		goto cleanup;
	}

	ret_code = sqlite3_open(SMARTCACHE_DB_PATH, &db);
	if (ret_code != SQLITE_OK) {
		write_log(4, "In %s. Failed to open database.", __func__);
		boost_fail = TRUE;
		goto cleanup;
	}

	ret_code = sqlite3_exec(db, sql, _iterate_pkg_cb,
				(void *)boost_job, &sql_err);
	if (ret_code != SQLITE_OK) {
		sqlite3_free(sql_err);
		sqlite3_close(db);
		boost_fail = TRUE;
		goto cleanup;
	}
	sqlite3_close(db);

	/* Start to boost/unboost */
	for (idx = 0; idx < boost_job->num_pkg; idx++) {
		package_name = boost_job->pkg_list[idx];
		if (boost_job->to_boost)
			ret_code = boost_package(package_name);
		else
			ret_code = unboost_package(package_name);
		if (ret_code < 0) {
			boost_fail = TRUE;
			goto cleanup;
		}
	}

cleanup:
	for (idx = 0; idx < boost_job->num_pkg; idx++) {
		free(boost_job->pkg_list[idx]);
	}
	free(boost_job->pkg_list);
	free(boost_job);

	if (!boost_fail) {
		write_log(4, "Finish process %s.",
			  (boost_job->to_boost) ? "boosting" : "unboosting");
		RETRY_SEND_EVENT(EVENT_BOOST_SUCCESS);
		pthread_exit(&(int){ 0 });
	} else {
		write_log(0, "Process %s failed.",
			  (boost_job->to_boost) ? "boosting" : "unboosting");
		RETRY_SEND_EVENT(EVENT_BOOST_FAILED);
		pthread_exit(&(int){ -1 });
	}
}

/* To process packages "boost"/"unboost". The list of packages will stored in
 * database maintained by Tera Mgmt APP.
 *
 * Note - This API will return immediately when receiving API request. HCFSAPID
 * will send an event to notify the result after "boost"/"unboost" finished or
 * failed.
 *
 * @to_boost 1 if process boosting. 0 if process unboosting.
 * @tid      thread to process boost job
 *
 * @return 0 if successful. Otherwise returns negation of error code
 */
int32_t trigger_boost(char to_boost, pthread_t *tid)
{
	char image_file_path[strlen(SMARTCACHE) + strlen(HCFSBLOCK) + 10];
	int32_t *boost;

	boost = (int32_t *)calloc(1, sizeof(int32_t));
	if (boost == NULL)
		return -ENOMEM;
	*boost = (to_boost) ? 1 : 0;

	snprintf(image_file_path, sizeof(image_file_path), "%s/%s", SMARTCACHE,
		 HCFSBLOCK);

	if (access(image_file_path, F_OK) == -1)
		return -ENOENT;

	pthread_create(tid, NULL, &process_boost, boost);

	return 0;
}

/* To check the boost status of a package.
 *
 * @return 0 if package is boosted.
 *         1 if package is unboosted.
 *         Otherwise, returns negation of error code.
 */
int32_t check_pkg_boost_status(char *package_name)
{
	char pkg_fullpath[strlen(package_name) + 20];
	int32_t ret_code;
	struct stat tmp_st;

	snprintf(pkg_fullpath, sizeof(pkg_fullpath), "%s/%s", DATA_PREFIX,
		 package_name);

	ret_code = lstat(pkg_fullpath, &tmp_st);
	if (ret_code < 0)
		return -errno;
	else if (S_ISLNK(tmp_st.st_mode))
		return 0; /* BOOST */
	else
		return 1; /* UNBOOST */

}

int32_t clear_boosted_package(char *package_name)
{
	int32_t ret_code;
	char path[500];

	/* Remove symbolic link */
	sprintf(path, "%s/%s", DATA_PREFIX, package_name);
	ret_code = unlink(path);
	if (ret_code < 0 && errno != ENOENT) {
		ret_code = -errno;
		return ret_code;
	}

	/* Remove target pkg folder */
	sprintf(path, "%s/%s", SMARTCACHEMTP, package_name);
	ret_code = rmdir(path);
	if (ret_code < 0 && errno != ENOENT)
		ret_code = -errno;
	else
		ret_code = 0;

	return ret_code;
}

int32_t toggle_smart_cache_mount(char to_mount)
{
	char cmd[1024];
	char cmd_mount_ext4_fs[] = "mount -t ext4 %s %s";
	char cmd_umount_ext4_fs[] = "umount %s";
	char cmd_setup_loop_device[] = "losetup %s %s";
	char cmd_restorecon[] = "restorecon %s";
	char image_file_path[strlen(SMARTCACHE) + strlen(HCFSBLOCK) + 10];
	int32_t status;

	if (to_mount) {
		snprintf(image_file_path, sizeof(image_file_path), "%s/%s",
			 SMARTCACHE, HCFSBLOCK);

		memset(cmd, 0, sizeof(cmd));
		snprintf(cmd, sizeof(cmd), cmd_setup_loop_device, LOOPDEV,
			 image_file_path);
		status = system(cmd);
		if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
			write_log(0, "In %s. Failed to run cmd %s", __func__,
				  cmd);
			return -1;
		}

		memset(cmd, 0, sizeof(cmd));
		snprintf(cmd, sizeof(cmd), cmd_mount_ext4_fs, LOOPDEV,
			 SMARTCACHEMTP);
		status = system(cmd);
		if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
			write_log(0, "In %s. Failed to run cmd %s", __func__,
				  cmd);
			return -1;
		}
	} else {
		memset(cmd, 0, sizeof(cmd));
		snprintf(cmd, sizeof(cmd), cmd_umount_ext4_fs, SMARTCACHEMTP);
		status = system(cmd);
		if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
			write_log(0, "In %s. Failed to run cmd %s", __func__,
				  cmd);
			return -1;
		}
	}

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), cmd_restorecon, SMARTCACHEMTP);
	status = system(cmd);
	if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
		write_log(0, "In %s. Failed to run cmd %s", __func__, cmd);
		return -1;
	}

	return 0;
}



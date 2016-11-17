/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: smart_cache.c
* Abstract: This c source file for smart cache APIs.
*
* Revision History
* 2016/10/15 Create this file.
*
**************************************************************************/

#include "smart_cache.h"

#include <sys/socket.h>
#include <sys/stat.h>
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

/* Helper function to mount hcfs volume
 *
 * @return Return code returned by hcfs api service.
 */
int32_t _mount_hcfs_vol()
{
	char buf[4096];
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

/* Helper function to remove nonempty folder by system cmd.
 */
int32_t _remove_folder(char *pathname)
{
	char cmd[4096];
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
	char cmd[4096];
	char cmd_create_image_file[] = "dd if=/dev/zero of=%s/%s bs=1 count=%d";
	char cmd_setup_loop_device[] = "losetup %s %s/%s";
	char cmd_create_ext4_fs[] = "make_ext4fs %s";
	char cmd_mount_ext4_fs[] = "mount -t ext4 %s %s";
	int32_t ret_code, status;

	ret_code = _create_hcfs_vol();
	if (ret_code < 0 && ret_code != -EEXIST) {
		write_log(0,
			  "In %s. Failed to create volume %s. Error code - %d",
			  __func__, SMARTCACHEVOL, ret_code);
		return ret_code;
	}

	unlink(SMARTCACHE);
	ret_code = mkdir(SMARTCACHE, 0771);
	if (ret_code < 0) {
		write_log(0, "In %s. Failed to mkdir %s. Error code - %d",
			  __func__, SMARTCACHE, ret_code);
		return ret_code;
	}
	/* chown to system:sytem */
	chown(SMARTCACHE, 1000, 1000);

	unlink(SMARTCACHEMTP);
	ret_code = mkdir(SMARTCACHEMTP, 0771);
	if (ret_code < 0) {
		write_log(0, "In %s. Failed to mkdir %s. Error code - %d",
			  __func__, SMARTCACHEMTP, ret_code);
		return ret_code;
	}

	ret_code = _mount_hcfs_vol();
	if (ret_code < 0) {
		write_log(0,
			  "In %s. Failed to mount volume %s. Error code - %d",
			  __func__, SMARTCACHEVOL, ret_code);
		return ret_code;
	}

	snprintf(cmd, sizeof(cmd), cmd_create_image_file, SMARTCACHE, HCFSBLOCK,
		 smart_cache_size);
	RUN_CMD_N_CHECK();

	snprintf(cmd, sizeof(cmd), cmd_setup_loop_device, LOOPDEV, SMARTCACHE,
		 HCFSBLOCK);
	RUN_CMD_N_CHECK();

	snprintf(cmd, sizeof(cmd), cmd_create_ext4_fs, LOOPDEV);
	RUN_CMD_N_CHECK();

	snprintf(cmd, sizeof(cmd), cmd_mount_ext4_fs, LOOPDEV, SMARTCACHEMTP);
	RUN_CMD_N_CHECK();

	return 0;
}

/* To move package data - Move data from /data/data/<pkg_folder> to smart cache.
 *
 * @return Return 0 if success. Otherwise, return -1.
 */
int32_t boost_package(char *package_name)
{
	char pkg_fullpath[strlen(package_name) + 20];
	char pkg_tmppath[strlen(package_name) + 25];
	char smart_cache_fullpath[strlen(SMARTCACHEMTP) + strlen(package_name)];
	char cmd[4096];
	char cmd_copy_pkg_data[] = "cp -r %s %s";
	int32_t ret_code, status;

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

	snprintf(cmd, sizeof(cmd), cmd_copy_pkg_data, pkg_fullpath,
		 SMARTCACHEMTP);
	status = system(cmd);
	if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
		write_log(0, "In %s. Failed to run cmd %s", __func__, cmd);
		goto rollback;
	}

	ret_code = rename(pkg_fullpath, pkg_tmppath);
	if (ret_code < 0) {
		write_log(0,
			  "In %s. Failed to create temp file %s. Error code %d",
			  __func__, pkg_tmppath, errno);
		goto rollback;
	}

	ret_code = symlink(smart_cache_fullpath, pkg_fullpath);
	if (ret_code < 0) {
		write_log(0, "In %s. Failed to create link %s. Error code %d",
			  __func__, smart_cache_fullpath, errno);
		goto rollback;
	}

	_remove_folder(pkg_tmppath);
	ret_code = 0;
	goto end;

rollback:
	if (access(smart_cache_fullpath, F_OK) != -1)
		_remove_folder(smart_cache_fullpath);

	if (access(pkg_tmppath, F_OK) != -1) {
		unlink(pkg_fullpath);
		rename(pkg_tmppath, pkg_fullpath);
	}
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
	char pkg_fullpath[strlen(package_name) + 20];
	char smart_cache_fullpath[strlen(SMARTCACHEMTP) + strlen(package_name)];
	char cmd[4096];
	char cmd_copy_pkg_data[] = "cp -r %s %s";
	int32_t ret_code, status;

	snprintf(pkg_fullpath, sizeof(pkg_fullpath), "%s/%s", DATA_PREFIX,
		 package_name);
	snprintf(smart_cache_fullpath, sizeof(smart_cache_fullpath), "%s/%s",
		 SMARTCACHEMTP, package_name);

	if (access(SMARTCACHEMTP, F_OK) == -1) {
		write_log(0, "In %s. Smart cache not existed.", __func__);
		return -1;
	}

	if (access(pkg_fullpath, F_OK) != -1) {
		write_log(4, "In %s. Pacakge path %s existed. Force remove it.",
			  __func__, pkg_fullpath);

		ret_code = _remove_folder(pkg_fullpath);
		if (ret_code < 0) {
			write_log(0, "In %s. Failed to run cmd %s", __func__,
				  cmd);
			return -1;
		}
	}

	memset(cmd, 0, sizeof(cmd));
	snprintf(cmd, sizeof(cmd), cmd_copy_pkg_data, smart_cache_fullpath,
		 pkg_fullpath);
	status = system(cmd);
	if ((!WIFEXITED(status)) || (WEXITSTATUS(status) != 0)) {
		write_log(0, "In %s. Failed to run cmd %s", __func__, cmd);
		goto rollback;
	}

	_remove_folder(smart_cache_fullpath);

	ret_code = 0;
	goto end;

rollback:
	if (access(pkg_fullpath, F_OK) != -1)
		_remove_folder(pkg_fullpath);
	ret_code = -1;

end:
	return ret_code;
}

/* To change boost status of package in database.
 */
int32_t change_boost_status(sqlite3 *db, char *pkg_name, int32_t status)
{
	int32_t ret_code;
	char *sql_err = 0;
	char sql[1024];

	snprintf(sql, sizeof(sql),
		 "Update %s SET boost_status=%d WHERE package_name='%s'",
		 SMARTCACHE_TABLE_NAME, status, pkg_name);
	ret_code = sqlite3_exec(db, sql, NULL, NULL, &sql_err);
	if (ret_code != SQLITE_OK) {
		write_log(0, "In %s. Error msg - %s\n", __func__, sql_err);
		sqlite3_free(sql_err);
		return -1;
	}

	return 0;
}

/* Sqlite callback function to iterate packages to boost/unboost.
 */
static int32_t _iterate_pkg_cb(void *data,
			       int32_t argc,
			       char **argv,
			       char **azColName)
{
	char to_boost;
	char *package_name;
	int32_t ret_code;
	BOOST_JOB_META *boost_job;
	sqlite3 *db;

	UNUSED(argc);
	UNUSED(azColName);

	if (!argv[0]) {
		write_log(0, "In %s. No matched result.", __func__);
		return -1;
	}
	package_name = argv[0];

	boost_job = (BOOST_JOB_META*)data;
	to_boost = boost_job->to_boost;
	db = boost_job->db;

	if (to_boost) {
		change_boost_status(db, package_name, ST_BOOSTING);
		ret_code = boost_package(package_name);
		if (ret_code < 0) {
			change_boost_status(db, package_name, ST_BOOST_FAILED);
			return -1;
		}
		change_boost_status(db, package_name, ST_BOOSTED);
	} else {
		change_boost_status(db, package_name, ST_UNBOOSTING);
		ret_code = unboost_package(package_name);
		if (ret_code < 0) {
			change_boost_status(db, package_name, ST_UNBOOST_FAILED);
			return -1;
		}
		change_boost_status(db, package_name, ST_UNBOOSTED);
	}

	return 0;
}

/* To query database for selected pacakge list and process packages
 * boosting/unboosting.
 */
void *process_boost(void *ptr)
{
	int32_t ret_code;
	char *sql_err = 0;
	char sql[1024];
	BOOST_JOB_META boost_job;

	boost_job.to_boost = (int32_t)ptr;

	/* Find target packages */
	if (boost_job.to_boost)
		snprintf(sql, sizeof(sql),
			 "SELECT package_name from %s WHERE boost_status=%d",
			 SMARTCACHE_TABLE_NAME, ST_INIT_BOOST);
	else
		snprintf(sql, sizeof(sql),
			 "SELECT package_name from %s WHERE boost_status=%d",
			 SMARTCACHE_TABLE_NAME, ST_INIT_UNBOOST);

	if (access(SMARTCACHE_DB_PATH, F_OK) != 0) {
		pthread_exit((void *)-1);
	}

	ret_code = sqlite3_open(SMARTCACHE_DB_PATH, &(boost_job.db));
	if (ret_code != SQLITE_OK) {
		pthread_exit((void *)-1);
	}

	ret_code = sqlite3_exec(boost_job.db, sql, _iterate_pkg_cb,
				(void *)&boost_job, &sql_err);
	if (ret_code != SQLITE_OK) {
		sqlite3_free(sql_err);
		sqlite3_close(boost_job.db);
		pthread_exit((void *)-1);
	}

	sqlite3_close(boost_job.db);
	pthread_exit((void *)0);
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

	snprintf(image_file_path, sizeof(image_file_path), "%s/%s", SMARTCACHE,
		 HCFSBLOCK);

	if (access(image_file_path, F_OK) == -1)
		return -ENOENT;

	pthread_create(tid, NULL, &process_boost, (void *)to_boost);
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

/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: minimal_apk.c
* Abstract: This c source file for minimal apk APIs.
*
* Revision History
* 2016/12/9 Create this file.
*
**************************************************************************/

#include "minimal_apk.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include <zip.h>

#include "global.h"
#include "logger.h"


/* Helper function to get dir name of a full path
 * e.g. Input: "/tmp/dir/file" => Output: "/tmp/dir"
 *
 * @return dir_name (Must be freed after used.)
 */
char *_get_dir_name(const char *file_name)
{
	char *ret_str;
	uint32_t count, idx;

	if (file_name == NULL)
		return NULL;

	idx = 0;
	for (count = 0; count < strlen(file_name); count++) {
		if (strncmp(&file_name[count], "/", 1) == 0)
			idx = count;
	}

	if (idx == 0)
		return NULL;

	ret_str = (char *)calloc(1, strlen(file_name) + 1);
	strncpy(ret_str, file_name, idx);
	return ret_str;
}

/* To add lib dirs to the archive of minimal apk.
 */
int32_t add_lib_dirs(zip_t *base_apk, zip_t *mini_apk)
{
	char *file_name;
	char *dir_name;
	int32_t ret_code, num_entries, idx;
	zip_error_t *zip_error;

	num_entries = zip_get_num_entries(base_apk, 0);
	if (num_entries < 0) {
		write_log(0, "Failed to parse base apk files");
		return -1;
	}

	for (idx = 0; idx < num_entries; idx++) {
		file_name = (char *)zip_get_name(base_apk, idx, 0);
		if (file_name == NULL) {
			write_log(0, "Failed to parse file names in base apk. "
				     "Error msg - %s",
				  zip_strerror(base_apk));
			return -1;
		}

		/* To find lib folders */
		if (strncmp(file_name, "lib/", 4) == 0) {
			dir_name = _get_dir_name(file_name);
			ret_code = zip_dir_add(mini_apk, dir_name, 0);
			if (ret_code < 0) {
				zip_error = zip_get_error(mini_apk);
				if (zip_error_code_zip(zip_error) !=
				    ZIP_ER_EXISTS) {
					write_log(0, "Failed to add lib dirs "
						     "to mini apk. "
						     "Error msg - %s",
						  zip_strerror(mini_apk));
					free(dir_name);
					return -1;
				}
			}
			free(dir_name);
		}
	}
	return 0;
}

/* To add AndroidManifest.xml & resources.arsc to minimal apk
 */
int32_t add_apk_files(zip_t *base_apk, zip_t *mini_apk)
{
	const char *apk_files[] = { F_MANIFEST, F_RESOURCE };
	uint32_t count;
	zip_int64_t idx_in_base;
	zip_source_t *tmp_zs_t;

	for (count = 0;
	     count < sizeof(apk_files) / sizeof(apk_files[0]); count++) {
		idx_in_base = zip_name_locate(base_apk, apk_files[count], 0);
		if (idx_in_base < 0) {
			write_log(
			    0, "Cann't find %s in base apk", apk_files[count]);
			return -1;
		}

		tmp_zs_t =
		    zip_source_zip(mini_apk, base_apk, idx_in_base, 0, 0, 0);
		if (tmp_zs_t == NULL) {
			write_log(0, "Failed to get %s source. "
				     "Error msg - %s",
				  apk_files[count], zip_strerror(mini_apk));
			return -1;
		}

		if (zip_add(mini_apk, apk_files[count], tmp_zs_t) == -1) {
			write_log(0, "Failed to add %s to mini apk. "
				     "Error msg - %s",
				  apk_files[count], zip_strerror(mini_apk));
			zip_source_free(tmp_zs_t);
			return -1;
		}
	}

	return 0;
}

/* To create the zip file of minimal apk which contains -
 *   1. AndroidManifest.xml
 *   2. resources.arsc
 *   3. lib folders
 *
 * @param pkg_name Target package (e.g. com.xxx.xxx-1)
 *
 * @return Return 0 if successful. Otherwise, return -1.
 */
int32_t create_zip_file(char *base_apk_path, char *mini_apk_path)
{
	int32_t ret_code;
	int32_t zip_errcode = 0;
	zip_t *base_apk_zip, *mini_apk_zip;
	zip_error_t zip_err_t;

	if ((base_apk_path == NULL) || (mini_apk_path == NULL)) {
		write_log(0, "Invalid path string.");
		return -1;
	}

	mini_apk_zip = zip_open(mini_apk_path, ZIP_CREATE, &zip_errcode);
	if (mini_apk_zip == NULL) {
		zip_error_init_with_code(&zip_err_t, zip_errcode);
		write_log(0, "Failed to open mini apk. Error msg - %s",
			  zip_error_strerror(&zip_err_t));
		zip_error_fini(&zip_err_t);
		return -1;
	}

	base_apk_zip = zip_open(base_apk_path, ZIP_CHECKCONS, &zip_errcode);
	if (base_apk_zip == NULL) {
		zip_error_init_with_code(&zip_err_t, zip_errcode);
		write_log(0, "Failed to open base apk. Error msg - %s",
			  zip_error_strerror(&zip_err_t));
		zip_error_fini(&zip_err_t);
		zip_close(mini_apk_zip);
		return -1;
	}

	ret_code = add_lib_dirs(base_apk_zip, mini_apk_zip);
	if (ret_code < 0) {
		write_log(0, "Failed to add lib dirs to minimal api.");
		zip_close(mini_apk_zip);
		zip_close(base_apk_zip);
		return -1;
	}

	add_apk_files(base_apk_zip, mini_apk_zip);
	if (ret_code < 0) {
		write_log(0, "Failed to add apk files to minimal api.");
		zip_close(mini_apk_zip);
		zip_close(base_apk_zip);
		return -1;
	}

	zip_close(mini_apk_zip);
	zip_close(base_apk_zip);
	return 0;
}

/* Main function to create minimal apk for an installed app.
 *
 * @param pkg_name Target pkg
 *
 * @return Return 0 if successful. Otherwise, return -1.
 */
int32_t create_minimal_apk(char *pkg_name)
{
	char *base_apk_path, *mini_apk_path;
	int32_t ret_code;
	struct stat tmp_stat;
	struct utimbuf base_apk_timbuf;
	struct group *grp_t;
	struct passwd *passwd_t;

	ret_code = 0;
	base_apk_path = mini_apk_path = NULL;

	if (pkg_name == NULL) {
		write_log(0, "Invalid pkg_name.");
		return -1;
	}

	ret_code = asprintf(&base_apk_path, "%s/%s/%s", APP_PREFIX, pkg_name,
			    BASE_APK_NAME);
	if (ret_code < 0) {
		write_log(0, "Failed to concat base apk path string.");
		return -1;
	}

	ret_code = asprintf(&mini_apk_path, "%s/%s/%s", APP_PREFIX, pkg_name,
			    MINI_APK_NAME);
	if (ret_code < 0) {
		write_log(0, "Failed to concat mini apk path string.");
		goto error;
	}

	if (access(mini_apk_path, F_OK) != -1) {
		write_log(
		    4, "Minimal apk of %s alreay existed. Skip apk creation.",
		    pkg_name);
		ret_code = 0;
		goto end;
	}

	ret_code = stat(base_apk_path, &tmp_stat);
	if (ret_code < 0) {
		write_log(0, "Failed to lookup base apk of %s. Error code - %d",
			  pkg_name, errno);
		goto error;
	}

	ret_code = create_zip_file(base_apk_path, mini_apk_path);
	if (ret_code < 0) {
		write_log(0, "Failed to create minimal apk file of %s.",
			  pkg_name);
		goto error;
	}

	/* change timestamps as same as base.apk */
	base_apk_timbuf.actime = tmp_stat.st_atime;
	base_apk_timbuf.modtime = tmp_stat.st_mtime;
	ret_code = utime(mini_apk_path, &base_apk_timbuf);
	if (ret_code < 0) {
		write_log(0, "Failed to change mod time for minimal apk of %s. "
			     "Error code - %d",
			  pkg_name, errno);
		unlink(mini_apk_path);
		goto error;
	}

	/* change owner & permisssions */
	grp_t = getgrnam("system");
	passwd_t = getpwnam("system");
	chown(mini_apk_path, passwd_t->pw_uid, grp_t->gr_gid);
	chmod(mini_apk_path, 0644);

	ret_code = 0;
	goto end;

error:
	ret_code = -1;

end:
	free(base_apk_path);
	free(mini_apk_path);
	return ret_code;
}

/* To create minimal apk asynchronously.
 */
void *create_minimal_apk_async(void *ptr)
{
	char *package_name;

	package_name = (char *)ptr;
	create_minimal_apk(package_name);
	free(package_name);

	pthread_exit(&(int){ 0 });
}

/* To check whether the minimal apk of an installed app is existed or not.
 *
 * @pkg_name target package.
 *
 * @return Return 0 if not existed, 1 if existed and 2 if creation is ongoing.
 */
int32_t check_minimal_apk(char *pkg_name)
{
	char *mini_apk_path;
	int32_t ret_code, apk_status;

	ret_code = asprintf(&mini_apk_path, "%s/%s/%s", APP_PREFIX, pkg_name,
			    MINI_APK_NAME);
	if (ret_code < 0) {
		write_log(0, "Failed to concat mini apk path string.");
		return -EINVAL;
	}

	if (access(mini_apk_path, F_OK) == -1)
		apk_status = ST_NOT_EXISTED;
	else
		apk_status = ST_EXISTED;

	free(mini_apk_path);
	return apk_status;
}

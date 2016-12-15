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


LL_ONGOING_MINI_APK *ongoing_mini_apk_list = NULL;
sem_t mini_apk_list_sem;

/* Helper function to initialize ongoing mini apk list
 */
void init_minimal_apk_list()
{
	sem_init(&mini_apk_list_sem, 0, 1);
	return;
}

void destroy_minimal_apk_list()
{
	sem_destroy(&mini_apk_list_sem);
	return;
}

/* Helper function to add an apk to ongoing list
 */
void _apk_list_add(char *pkg_name) {
	LL_ONGOING_MINI_APK *apk_entry, *curr_entry;

	apk_entry =
	    (LL_ONGOING_MINI_APK *)calloc(1, sizeof(LL_ONGOING_MINI_APK));
	apk_entry->pkg_name = strdup(pkg_name);
	apk_entry->next = NULL;

	if (apk_entry->pkg_name == NULL) {
		WRITE_LOG(
		    0, "Failed to alloc memory for new apk entry. Error - %d",
		    errno);
		return;
	}

	if (ongoing_mini_apk_list == NULL) {
		ongoing_mini_apk_list = apk_entry;
		return;
	}

	curr_entry = ongoing_mini_apk_list;
	while (curr_entry->next != NULL) {
		curr_entry = curr_entry->next;
	}

	curr_entry->next = apk_entry;

	return;
}

/* Helper function to check whether an apk is ongoing or not
 *
 * @return Return TRUE if in list. Return FALSE if not in list.
 */
BOOL _apk_list_search(char *pkg_name)
{
	BOOL pkg_existed = FALSE;
	LL_ONGOING_MINI_APK *curr_entry;

	curr_entry = ongoing_mini_apk_list;
	while (curr_entry != NULL) {
		if (strncmp(curr_entry->pkg_name, pkg_name, strlen(pkg_name)) ==
		    0) {
			pkg_existed = TRUE;
			break;
		}
		curr_entry = curr_entry->next;
	}

	return pkg_existed;
}

/* Helper function to remove an apk in list
 */
void _apk_list_remove(char *pkg_name) {
	BOOL pkg_existed = FALSE;
	int32_t count = 0;
	LL_ONGOING_MINI_APK *prev_entry, *curr_entry;

	if (ongoing_mini_apk_list == NULL)
		return;

	prev_entry = curr_entry = ongoing_mini_apk_list;
	while (curr_entry != NULL) {
		if (strncmp(curr_entry->pkg_name, pkg_name,
			    strlen(pkg_name)) == 0) {
			pkg_existed = TRUE;
			break;
		}
		prev_entry = curr_entry;
		curr_entry = prev_entry->next;
		count++;
	}

	if (pkg_existed) {
		if (count == 0) {
			ongoing_mini_apk_list = curr_entry->next;
		} else {
			prev_entry->next = curr_entry->next;
		}
		free(curr_entry->pkg_name);
		free(curr_entry);
	}

	return;
}

/* Helper function to get dir name of a full path
 * e.g. Input: "/tmp/dir/file" => Output: "/tmp/dir"
 *
 * @return dir_name (Must be freed after used.)
 */
char *_get_dir_name(const char *file_name)
{
	char *ret_str;
	int32_t idx;
	uint32_t count;

	if (file_name == NULL)
		return NULL;

	idx = -1;
	for (count = 0; count < strlen(file_name); count++) {
		if (file_name[count] == '/')
			idx = count;
	}

	if (idx == -1) {
		ret_str = (char *)calloc(1, 1);
	} else if (idx == 0) {
		ret_str = (char *)calloc(1, 2);
		ret_str[0] = '/';
	} else {
		ret_str = (char *)calloc(1, idx + 1);
		strncpy(ret_str, file_name, idx);
	}

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
		WRITE_LOG(0, "Failed to parse base apk files");
		return -1;
	}

	for (idx = 0; idx < num_entries; idx++) {
		file_name = (char *)zip_get_name(base_apk, idx, 0);
		if (file_name == NULL) {
			WRITE_LOG(0, "Failed to parse file names in base apk. "
				     "Error msg - %s",
				  zip_strerror(base_apk));
			return -1;
		}

		/* To find lib folders */
		if (strncmp(file_name, "lib/", 4) != 0)
			continue;

		dir_name = _get_dir_name(file_name);
		ret_code = zip_dir_add(mini_apk, dir_name, 0);
		if (ret_code < 0) {
			zip_error = zip_get_error(mini_apk);
			if (zip_error_code_zip(zip_error) != ZIP_ER_EXISTS) {
				WRITE_LOG(0, "Failed to add lib dirs "
					     "to mini apk. "
					     "Error msg - %s",
					  zip_strerror(mini_apk));
				free(dir_name);
				return -1;
			}
		}
		free(dir_name);
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
			WRITE_LOG(
			    0, "Cann't find %s in base apk", apk_files[count]);
			return -1;
		}

		tmp_zs_t =
		    zip_source_zip(mini_apk, base_apk, idx_in_base, 0, 0, 0);
		if (tmp_zs_t == NULL) {
			WRITE_LOG(0, "Failed to get %s source. "
				     "Error msg - %s",
				  apk_files[count], zip_strerror(mini_apk));
			return -1;
		}

		if (zip_add(mini_apk, apk_files[count], tmp_zs_t) == -1) {
			WRITE_LOG(0, "Failed to add %s to mini apk. "
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
		WRITE_LOG(0, "Invalid path string.");
		return -1;
	}

	mini_apk_zip = zip_open(mini_apk_path, ZIP_CREATE, &zip_errcode);
	if (mini_apk_zip == NULL) {
		zip_error_init_with_code(&zip_err_t, zip_errcode);
		WRITE_LOG(0, "Failed to open mini apk. Error msg - %s",
			  zip_error_strerror(&zip_err_t));
		zip_error_fini(&zip_err_t);
		return -1;
	}

	base_apk_zip = zip_open(base_apk_path, ZIP_CHECKCONS, &zip_errcode);
	if (base_apk_zip == NULL) {
		zip_error_init_with_code(&zip_err_t, zip_errcode);
		WRITE_LOG(0, "Failed to open base apk. Error msg - %s",
			  zip_error_strerror(&zip_err_t));
		zip_error_fini(&zip_err_t);
		zip_close(mini_apk_zip);
		return -1;
	}

	ret_code = add_lib_dirs(base_apk_zip, mini_apk_zip);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to add lib dirs to minimal api.");
		zip_close(mini_apk_zip);
		zip_close(base_apk_zip);
		return -1;
	}

	add_apk_files(base_apk_zip, mini_apk_zip);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to add apk files to minimal api.");
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
	char *base_apk_path, *mini_apk_path, *app_dir_path;
	int32_t ret_code;
	struct stat base_apk_stat, app_dir_stat;
	struct utimbuf base_apk_timbuf, app_dir_timbuf;
	struct group *grp_t;
	struct passwd *passwd_t;

	ret_code = 0;
	base_apk_path = mini_apk_path = app_dir_path = NULL;

	if (pkg_name == NULL) {
		WRITE_LOG(0, "Invalid pkg_name.");
		return -1;
	}

	sem_wait(&mini_apk_list_sem);
	if (_apk_list_search(pkg_name)) {
		sem_post(&mini_apk_list_sem);
		return 0;
	}
	_apk_list_add(pkg_name);
	sem_post(&mini_apk_list_sem);

	ret_code = asprintf(&base_apk_path, "%s/%s/%s", APP_PREFIX, pkg_name,
			    BASE_APK_NAME);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to concat base apk path string.");
		return -1;
	}

	ret_code = asprintf(&mini_apk_path, "%s/%s/%s", APP_PREFIX, pkg_name,
			    MINI_APK_NAME);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to concat mini apk path string.");
		goto error;
	}

	if (access(mini_apk_path, F_OK) != -1) {
		WRITE_LOG(
		    4, "Minimal apk of %s alreay existed. Skip apk creation.",
		    pkg_name);
		ret_code = 0;
		goto end;
	}

	app_dir_path = _get_dir_name(base_apk_path);
	ret_code = stat(app_dir_path, &app_dir_stat);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to lookup app dir of %s. Error code - %d",
			  pkg_name, errno);
		goto error;
	}

	ret_code = stat(base_apk_path, &base_apk_stat);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to lookup base apk of %s. Error code - %d",
			  pkg_name, errno);
		goto error;
	}

	/* create minimal apk file */
	ret_code = create_zip_file(base_apk_path, mini_apk_path);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to create minimal apk file of %s.",
			  pkg_name);
		goto error;
	}

	/* change owner & permisssions */
	CHOWN(mini_apk_path, "system", "system");
	chmod(mini_apk_path, 0644);

	/* restore mod time */
	UTIME(mini_apk_path, base_apk_timbuf, base_apk_stat);
	UTIME(app_dir_path, app_dir_timbuf, app_dir_stat);

	ret_code = 0;
	goto end;

error:
	ret_code = -1;

end:
	free(base_apk_path);
	free(mini_apk_path);
	free(app_dir_path);
	/* Remove from ongoing list */
	sem_wait(&mini_apk_list_sem);
	_apk_list_remove(pkg_name);
	sem_post(&mini_apk_list_sem);
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
		WRITE_LOG(0, "Failed to concat mini apk path string.");
		return -EINVAL;
	}

	sem_wait(&mini_apk_list_sem);

	if (_apk_list_search(pkg_name))
		apk_status = ST_IS_CREATING;
	else if (access(mini_apk_path, F_OK) == -1)
		apk_status = ST_NOT_EXISTED;
	else
		apk_status = ST_EXISTED;

	sem_post(&mini_apk_list_sem);

	free(mini_apk_path);
	return apk_status;
}

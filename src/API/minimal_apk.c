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

/* Helper function for converting apk name to minimal apk name */
int32_t _convert_minapk(const char *apkname, char *minapk_name)
{
	size_t name_len;

	name_len = strlen(apkname);

	/* The length to copy before ".apk" */
	name_len -= 4;
	snprintf(minapk_name, (name_len + 2), ".%s", apkname);
	snprintf(&(minapk_name[1 + name_len]), 4, "min");
	return 0;
}

int32_t _convert_origin_apk(char *apkname, const char *minapk_name)
{
	size_t name_len;

	name_len = strlen(minapk_name);

	/* Could not be an apk */
	if (name_len < 5)
		return -EINVAL;

	/* From .<x>min to <x>.apk */
	memcpy(apkname, minapk_name + 1, name_len - 4);
	memcpy(apkname + name_len - 4, ".apk", 4);
	apkname[name_len] = '\0';
	return 0;
}


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
		if (ret_str == NULL) 
			return NULL;
		ret_str[0] = '/';
	} else {
		ret_str = (char *)calloc(1, idx + 1);
		if (ret_str == NULL)
			return NULL;
		strncpy(ret_str, file_name, idx);
	}

	return ret_str;
}

/* To add lib dirs to the archive of minimal apk.
 */
BOOL _is_possible_icon_name_substr(const char *file_name,
			    const MINI_APK_NEEDED *min_apk_needed)
{
	BOOL result = FALSE;
	int i;

	for (i = 0; i < min_apk_needed->num_icon; i++) {
		if (strstr(file_name, min_apk_needed->reserved_icon_names[i])) {
			WRITE_LOG(8, "Debug: %s is valid icon name\n",
				  file_name);
			result = TRUE;
			break;
		}
	}
	return result;
}

BOOL _is_possible_icon_name(const char *file_name,
			    const MINI_APK_NEEDED *min_apk_needed)
{
	BOOL result = FALSE;
	char icon_name[500] = {0};
	int slash_idx = -1, dot_idx = -1, file_name_len = -1;
	int i;

	// E.g: fetch string "icon" from string "/xxx/yyy/icon.png"
	for (i = strlen(file_name) - 1; i >= 0; i--) {
		if (file_name[i] == '.' && dot_idx == -1)
			dot_idx = i;
		if (file_name[i] == '/' && slash_idx == -1)
			slash_idx = i;
		if (dot_idx > 0 && slash_idx > 0) {
			if (dot_idx > slash_idx) {
				file_name_len = dot_idx - slash_idx - 1;
				memcpy(icon_name, file_name + slash_idx + 1,
				       file_name_len);
			} else {
				break;
			}
		}
	}
	if (icon_name[0] == 0)
		return FALSE;

	for (i = 0; i < min_apk_needed->num_icon; i++) {
		if (strcmp(icon_name, min_apk_needed->reserved_icon_names[i]) ==
		    0) {
			WRITE_LOG(8, "Debug: %s is valid icon name\n",
				  file_name);
			result = TRUE;
			break;
		}
	}
	return result;
}

int32_t add_lib_dirs_and_icon(zip_t *base_apk,
			      zip_t *mini_apk,
			      const MINI_APK_NEEDED *min_apk_needed)
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
		if (strncmp(file_name, "lib/", 4) == 0) {
			dir_name = _get_dir_name(file_name);
			if (dir_name == NULL) {
				WRITE_LOG(0, "Failed to allocate mem");
				return -1;
			}
			ret_code = zip_dir_add(mini_apk, dir_name, 0);
			if (ret_code < 0) {
				zip_error = zip_get_error(mini_apk);
				if (zip_error_code_zip(zip_error) !=
				    ZIP_ER_EXISTS) {
					WRITE_LOG(0, "Failed to add lib dirs "
						     "to mini apk. "
						     "Error msg - %s",
						  zip_strerror(mini_apk));
					free(dir_name);
					return -1;
				}
			}
			free(dir_name);
		} else if (_is_possible_icon_name(file_name, min_apk_needed)) {
			zip_source_t *tmp_zs_t =
			    zip_source_zip(mini_apk, base_apk, idx, 0, 0, 0);
			if (!tmp_zs_t) {
				WRITE_LOG(0, "Failed to get %s source. "
					     "Error msg - %s",
					  file_name, zip_strerror(mini_apk));
				return -1;
			}
			if (zip_add(mini_apk, file_name, tmp_zs_t) == -1) {
				WRITE_LOG(0, "Failed to add %s to mini apk. "
					     "Error msg - %s",
					  file_name, zip_strerror(mini_apk));
				zip_source_free(tmp_zs_t);
				return -1;
			}
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
int32_t create_zip_file(char *base_apk_path,
			char *mini_apk_path,
			const MINI_APK_NEEDED *min_apk_needed)
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

	ret_code =
	    add_lib_dirs_and_icon(base_apk_zip, mini_apk_zip, min_apk_needed);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to add lib dirs to minimal apk.");
		zip_close(mini_apk_zip);
		zip_close(base_apk_zip);
		return -1;
	}

	ret_code = add_apk_files(base_apk_zip, mini_apk_zip);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to add apk files to minimal apk.");
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
int32_t create_minimal_apk(MINI_APK_NEEDED *min_apk_needed)
{
	char *base_apk_path, *mini_apk_path, *app_dir_path;
	int32_t ret_code;
	struct stat base_apk_stat, app_dir_stat;
	struct utimbuf base_apk_timbuf, app_dir_timbuf;
	struct group *grp_t;
	struct passwd *passwd_t;
	char *pkg_name = min_apk_needed->pkg_name;

	ret_code = 0;
	base_apk_path = mini_apk_path = app_dir_path = NULL;
	WRITE_LOG(0, "Creating min apk for %s\n", pkg_name);

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

	ret_code = asprintf(&app_dir_path, "%s/%s", APP_PREFIX, pkg_name);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to concat package path string.");
		goto error;
	}

	int errcode = 0;
	int is_kitkat = FALSE;
	ret_code = stat(app_dir_path, &app_dir_stat);
	if (ret_code < 0) {
		errcode = -errno;
		WRITE_LOG(0, "Failed to lookup app dir of %s. Error code - %d",
			  pkg_name, -errcode);
		goto error;
	}

	/* Check if this is folder struct for Kitkat */
	if (!S_ISDIR(app_dir_stat.st_mode))
		is_kitkat = TRUE;

	if (is_kitkat == TRUE)
		ret_code = asprintf(&base_apk_path, "%s/%s",
		                    APP_PREFIX, pkg_name);
	else
		ret_code = asprintf(&base_apk_path, "%s/%s/%s", APP_PREFIX,
		                    pkg_name, BASE_APK_NAME);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to concat base apk path string.");
		goto error;
	}

	if (is_kitkat == TRUE) {
		char minapkname[256];
		_convert_minapk(pkg_name, minapkname);
		ret_code = asprintf(&mini_apk_path, "%s/%s",
		                    APP_PREFIX, minapkname);
	} else {
		ret_code = asprintf(&mini_apk_path, "%s/%s/%s",
			    APP_PREFIX, pkg_name, MINI_APK_NAME);
	}

	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to concat mini apk path string.");
		goto error;
	}
	WRITE_LOG(0, "Debug paths: %s %s %s %s", pkg_name, base_apk_path, mini_apk_path, app_dir_path);

	if (access(mini_apk_path, F_OK) != -1) {
		WRITE_LOG(
		    4, "Minimal apk of %s alreay existed. Skip apk creation.",
		    pkg_name);
		ret_code = 0;
		goto end;
	}

	ret_code = stat(base_apk_path, &base_apk_stat);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to lookup base apk of %s (%s). Error code - %d",
			  pkg_name, base_apk_path, errno);
		goto error;
	}

	/* create minimal apk file */
	ret_code =
	    create_zip_file(base_apk_path, mini_apk_path, min_apk_needed);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to create minimal apk file of %s (%s).",
			  pkg_name, mini_apk_path);
		goto error;
	}

	/* change owner & permisssions */
	CHOWN(mini_apk_path, "system", "system");
	chmod(mini_apk_path, 0644);

	/* restore mod time */
	UTIME(mini_apk_path, base_apk_timbuf, base_apk_stat);
	if (is_kitkat == FALSE)
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
	MINI_APK_NEEDED *min_apk_needed = (MINI_APK_NEEDED *)ptr;

	create_minimal_apk(min_apk_needed);
	destroy_minimal_apk_list(min_apk_needed);

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
	char *mini_apk_path = NULL, *app_dir_path = NULL;
	int32_t ret_code, apk_status = ST_NOT_EXISTED;

	ret_code = asprintf(&app_dir_path, "%s/%s", APP_PREFIX, pkg_name);
	if (ret_code < 0) {
		WRITE_LOG(0, "Failed to concat package path string.");
		return -ENOMEM;
	}

	int errcode = 0;
	int is_kitkat = FALSE;
	ret_code = access(app_dir_path, F_OK);
	if (ret_code < 0) {
		errcode = -errno;
		if (errcode == -ENOENT) {
			/* This might be Kitkat. */
			is_kitkat = TRUE;
			ret_code = 0;
		} else {
			WRITE_LOG(0, "Failed to lookup app dir of %s. Error code - %d",
				  pkg_name, -errcode);
			return errcode;
		}
	}

	if (is_kitkat == TRUE) {
		char minapkname[256];
		_convert_minapk(pkg_name, minapkname);
		ret_code = asprintf(&mini_apk_path, "%s/%s",
		                    APP_PREFIX, minapkname);
	} else {
		ret_code = asprintf(&mini_apk_path, "%s/%s/%s",
			    APP_PREFIX, pkg_name, MINI_APK_NAME);
	}

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
	free(app_dir_path);
	return apk_status;
}

MINI_APK_NEEDED *create_min_apk_needed_data(char *buf)
{
	ssize_t str_len;
	char *package_name;
	MINI_APK_NEEDED *min_apk_needed = NULL;
	char **icon_name_list;
	int32_t idx, now_pos, num_icon;

	memcpy(&str_len, buf, sizeof(ssize_t));

	package_name = (char *)calloc(1, str_len + 10);
	if (package_name == NULL) {
		write_log(0, "Failed to alloc memory for package name.");
		errno = ENOMEM;
		return NULL;
	}

	strncpy(package_name, buf + sizeof(ssize_t), str_len);
	write_log(0, "test create min: %s", package_name);

	/* Copy icon names */
	now_pos = sizeof(ssize_t) + str_len;
	memcpy(&num_icon, buf + now_pos, sizeof(int32_t));
	if (num_icon < 0) {
		write_log(0, "Error on invalid icon number %d\n", num_icon);
		free(package_name);
		errno = EINVAL;
		return NULL;
	}
	write_log(0, "num of icon name %d", num_icon);
	now_pos += sizeof(int32_t);
	icon_name_list = (char **)calloc(sizeof(char *) * num_icon, 1);
	for (idx = 0; idx < num_icon; idx++) {
		ssize_t icon_name_len;

		/* [ssize_t] [icon name 1] [ssize_t] [icon_name 2] .... */
		memcpy(&icon_name_len, buf + now_pos, sizeof(ssize_t));
		now_pos += sizeof(ssize_t);
		if (*(buf + now_pos + icon_name_len - 1) != '\0') {
			write_log(0, "Error: Invalid length of icon name %s",
				  buf + now_pos);
			errno = EINVAL;
			return NULL;
		}
		icon_name_list[idx] = (char *)calloc(icon_name_len + 10, 1);
		strncpy(icon_name_list[idx], buf + now_pos, icon_name_len);
		now_pos += icon_name_len;
		write_log(8, "Debug: parse icon name %s", icon_name_list[idx]);
	}

	min_apk_needed = (MINI_APK_NEEDED *)calloc(sizeof(MINI_APK_NEEDED), 1);
	min_apk_needed->pkg_name = package_name;
	min_apk_needed->reserved_icon_names = icon_name_list;
	min_apk_needed->num_icon = num_icon;
	return min_apk_needed;
}

void destroy_min_apk_needed_data(MINI_APK_NEEDED *min_apk_needed)
{
	int32_t i;

	free(min_apk_needed->pkg_name);
	for (i = 0; i < min_apk_needed->num_icon; i++)
		free(min_apk_needed->reserved_icon_names[i]);
	free(min_apk_needed->reserved_icon_names);
	free(min_apk_needed);
}

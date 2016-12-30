/*************************************************************************
*
* Copyright © 2016-2017 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfscurl.h
* Abstract: The c header file for CURL operations.
*
* Revision History
* 2016/12/29 Kewei created this source file.
*
**************************************************************************/

#include "googledrive_curl.h"

#include <unistd.h>

#include "hcfscurl.h"
#include "macro.h"
#include "monitor.h"
#include "fuseop.h"

int32_t init_gdrive_token_control(void)
{
	if (googledrive_token_control)
		return -EEXIST;

	googledrive_token_control =
	    (BACKEND_TOKEN_CONTROL *)calloc(sizeof(BACKEND_TOKEN_CONTROL), 1);
	if (!googledrive_token_control)
		return -errno;

	pthread_mutex_init(&googledrive_token_control->access_lock, NULL);
	pthread_mutex_init(&googledrive_token_control->waiting_lock, NULL);
	pthread_cond_init(&googledrive_token_control->waiting_cond, NULL);

	return 0;
}

int32_t hcfs_gdrive_reauth(CURL_HANDLE *curl_handle)
{
	int32_t ret_code;

	if (curl_handle->curl != NULL)
		hcfs_destroy_gdrive_backend(curl_handle->curl);

	curl_handle->curl = curl_easy_init();

	if (swift_auth_string[0] != 0) {
		ret_code = pthread_mutex_trylock(
		    &(googledrive_token_control->access_lock));
		if (ret_code == 0) {
			memset(googledrive_token, 0, sizeof(swift_auth_string));
			pthread_mutex_unlock(
			    &(googledrive_token_control->access_lock));
		}
	}

	if (curl_handle->curl)
		return hcfs_get_auth_token();

	return -1;

}

int32_t hcfs_init_gdrive_backend(CURL_HANDLE *curl_handle)
{
	int32_t ret_code;

	curl_handle->curl = curl_easy_init();

	if (curl_handle->curl) {
		ret_code = hcfs_get_auth_token();
		if (ret_code == 200 || googledrive_token[0] != 0)
			return 200;
		else
			return ret_code;
	}
	return -1;
}

int32_t hcfs_gdrive_list_container(FILE *fptr, CURL_HANDLE *curl_handle,
				   GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;
	char *filter_string = NULL, *title_string = NULL;
	FILE *gdrive_header_fptr, *gdrive_list_body_fptr;
	CURL *curl;
	char header_filename[200];
	int32_t ret_val, ret, num_retries, errcode;
	BOOL title_exist = FALSE;

	if (googledrive_token[0] == 0)
		return 401;

	if (!fptr) {
		write_log(4, "content_fptr cannot be null in %s.", __func__);
		return -1;
	}

	sprintf(header_filename, "/dev/shm/googledrivelisthead%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	gdrive_header_fptr = fopen(header_filename, "w+");
	if (!gdrive_header_fptr) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	setbuf(gdrive_header_fptr, NULL);
	gdrive_list_body_fptr = fptr;

	/* Create filter string */
	if (obj_info->file_title[0]) {
		ASPRINTF(&title_string, "q=title+contains+'%s'",
			obj_info->file_title);
		title_exist = TRUE;
	}
	if (obj_info->parentID[0]) {
		if (title_exist) {
			ASPRINTF(&filter_string, "%s+and+'%s'+in+parents",
				 title_string, obj_info->parentID);
			FREE(title_string);
		} else {
			ASPRINTF(&filter_string, "q='%s'+in+parents",
				 obj_info->parentID);
		}
	}

	/* Create URL */
	if (filter_string) {
	printf("%s\n", filter_string);
		ASPRINTF(&url, "https://www.googleapis.com/drive/v2/files?%s",
			 filter_string);
		FREE(filter_string);
	} else {
		ASPRINTF(&url, "https://www.googleapis.com/drive/v2/files");
	}

	chunk = NULL;
	chunk = curl_slist_append(chunk, googledrive_token);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, gdrive_header_fptr);

	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, gdrive_list_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);
	
	HTTP_PERFORM_RETRY(curl);
	//update_backend_status((res == CURLE_OK), NULL);
	FREE(url);
	curl_slist_free_all(chunk);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(gdrive_header_fptr);
		unlink(header_filename);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	/* Parse header */
	ret_val = parse_http_header_retcode(gdrive_header_fptr);
	if (ret_val < 0) {
		char header[1024] = {0};

		fseek(gdrive_header_fptr, 0, SEEK_SET);
		fread(header, sizeof(char), 1000, gdrive_header_fptr);
		write_log(5, "Warn: Fail to parse list header:\n%s", header);
		fclose(gdrive_header_fptr);
		unlink(header_filename);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		//update_backend_status(FALSE, NULL);
	if (!http_is_success(ret_val))
		ftruncate(fileno(fptr), 0);

	fclose(gdrive_header_fptr);
	gdrive_header_fptr = NULL;
	//UNLINK(header_filename);
	return ret_val;

errcode_handle:
	FREE(title_string);
	FREE(filter_string);
	FREE(url);
	if (gdrive_header_fptr != NULL) {
		fclose(gdrive_header_fptr);
		unlink(header_filename);
		/* curl_slist_free_all(chunk); */
	}

	return -1;
}

void hcfs_destroy_gdrive_backend(CURL *curl) { curl_easy_cleanup(curl); }

__attribute__((constructor))
void test()
{
	CURL_HANDLE curl_handle;
	GOOGLEDRIVE_OBJ_INFO info;
	FILE *fptr;

	curl_handle.curl = curl_easy_init();
	curl_handle.curl_backend = GOOGLEDRIVE;
	strcpy(curl_handle.id, "test");
	strcpy(googledrive_token, "Authorization:Bearer ya29."
				  "CjDEA36uslgODtFOXF5liy8dTj2wBayJUFkIyn2LRC6Q"
				  "pxUM-Kmuud3KDnWeH12M7hk");

	info.fileID[0] = 0;
	info.parentID[0] = 0;
	strcpy(info.file_title, "meta12");
	strcpy(info.parentID, "0B9tmNFTdZf0beEo0MW5MQXNtTXM");
	
	fptr = fopen("content", "w+");
	setbuf(fptr, NULL);

	int32_t ret = hcfs_gdrive_list_container(fptr, &curl_handle, &info);
	printf("ret = %d", ret);
	fclose(fptr);
	exit(0);
}


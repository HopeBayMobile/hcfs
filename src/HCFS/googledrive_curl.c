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

#include <jansson.h>
#include <unistd.h>
#include <errno.h>

#include "hcfscurl.h"
#include "macro.h"
#include "monitor.h"
#include "fuseop.h"

size_t read_post_file_function(void *ptr, size_t size, size_t nmemb,
			  void *post_control1)
{
	FILE *fptr;
	size_t actual_to_read;
	int32_t errcode;
	size_t ret_size;
	size_t expect_read, total_read = 0;
	object_post_control *post_control;

	post_control = (object_post_control *)post_control1;

	if (post_control->total_remaining <= 0)
		return 0;

	total_read = 0;
	expect_read = size * nmemb;
	if (expect_read <= 0)
		return total_read;

	/* Read head */
	if (post_control->head_remaining > 0) {
		if (expect_read > (size_t)post_control->head_remaining)
			actual_to_read = post_control->head_remaining;
		else
			actual_to_read = expect_read;
		memcpy((char *)ptr, post_control->head_string, actual_to_read);
		post_control->head_remaining -= actual_to_read;
		post_control->total_remaining -= actual_to_read;
		expect_read -= actual_to_read;
		total_read += actual_to_read;
		if (expect_read <= 0)
			return total_read;
	}

	/* Body */
	if (post_control->object_remaining_size > 0 &&
	    post_control->fptr != NULL) {
		fptr = post_control->fptr;
		if (expect_read > (size_t)post_control->object_remaining_size)
			actual_to_read = post_control->object_remaining_size;
		else
			actual_to_read = expect_read;
		FREAD((char *)ptr + total_read, 1, actual_to_read, fptr);
		post_control->object_remaining_size -= ret_size;
		post_control->total_remaining -= ret_size;
		expect_read -= ret_size;
		total_read += ret_size;
		if (expect_read <= 0)
			return total_read;
	}

	/* Tail */
	if (post_control->tail_remaining > 0) {
		if (expect_read > (size_t)post_control->tail_remaining)
			actual_to_read = post_control->tail_remaining;
		else
			actual_to_read = expect_read;
		memcpy((char *)ptr + total_read, post_control->tail_string,
				actual_to_read);
		post_control->tail_remaining -= actual_to_read;
		post_control->total_remaining -= actual_to_read;
		expect_read -= actual_to_read;
		total_read += actual_to_read;
	}

	return total_read;

errcode_handle:
	errno = errcode;
	return 0;
}

int32_t init_gdrive_token_control(void)
{
	if (googledrive_token_control)
		return -EEXIST;

	/* Init folder id cache */
	gdrive_folder_id_cache = (GOOGLE_DRIVE_FOLDER_ID_CACHE_T *)calloc(
	    sizeof(GOOGLE_DRIVE_FOLDER_ID_CACHE_T), 1);
	if (!gdrive_folder_id_cache)
		return -errno;
	sem_init(&(gdrive_folder_id_cache->op_lock), 0, 1);

	/* Init google drive token controller */
	googledrive_token_control =
	    (BACKEND_TOKEN_CONTROL *)calloc(sizeof(BACKEND_TOKEN_CONTROL), 1);
	if (!googledrive_token_control) {
		FREE(gdrive_folder_id_cache);
		return -errno;
	}

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

	if (googledrive_token[0] != 0) {
		ret_code = pthread_mutex_trylock(
		    &(googledrive_token_control->access_lock));
		if (ret_code == 0) {
			memset(googledrive_token, 0, sizeof(googledrive_token));
			pthread_mutex_unlock(
			    &(googledrive_token_control->access_lock));
		}
	}

	if (curl_handle->curl)
		return hcfs_get_auth_token();

	return -1;

}

int32_t hcfs_gdrive_test_backend(CURL_HANDLE *curl_handle)
{
	/*TODO: How to actually export the list of objects to other functions*/
	struct curl_slist *chunk = NULL;
	CURLcode res;
	FILE *gdrive_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, errcode;
	char googledrive_test_url[200];

	/* For GOOGLEDRIVE backend - token not set situation */
	if (googledrive_token[0] == 0)
		return 401;

	snprintf(header_filename, sizeof(header_filename),
		 "/dev/shm/googledrive_accounthead%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	gdrive_header_fptr = fopen(header_filename, "w+");
	if (gdrive_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	snprintf(googledrive_test_url, 200, "https://www.googleapis.com/drive/v3/files");

	chunk = NULL;
	chunk = curl_slist_append(chunk, googledrive_token);
	chunk = curl_slist_append(chunk, "Expect:");

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, googledrive_test_url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, gdrive_header_fptr);

	curl_easy_setopt(curl, CURLOPT_TIMEOUT, MONITOR_TEST_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

	res = curl_easy_perform(curl);

	if (res == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ret_val);
	else
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));

	fclose(gdrive_header_fptr);
	unlink(header_filename);
	curl_slist_free_all(chunk);

	if (res == CURLE_OK)
		return ret_val;
	else
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

void _create_folder_json_content(char *head,
			    char *tail,
			    const char *file_title,
			    const char *parentID)
{
	if (parentID[0]) {
		sprintf(head,
			"{"
			"\"title\":\"%s\", "
			"\"parents\":[{\"id\":\"%s\"}], "
			"\"mimeType\": \"application/vnd.google-apps.folder\""
			"}\n",
			file_title, parentID);
	} else {
		sprintf(head,
			"{"
			"\"title\":\"%s\", "
			"\"mimeType\": \"application/vnd.google-apps.folder\""
			"}\n",
			file_title);
	}
	tail[0] = 0;
}
void _create_multipart_data(char *head,
			    char *tail,
			    const char *file_title,
			    const char *parentID,
			    const char *boundary_string)
{
	//parentID = NULL;
	if (parentID[0]) {
		sprintf(head, "--%s\n"
			      "Content-Type: application/json; "
			      "charset=UTF-8\n\n"
			      "{"
			      "\"title\":\"%s\", "
			      "\"parents\":[{\"id\":\"%s\"}]"
			      "}\n\n"
			      "--%s\n\n",
			boundary_string, file_title, parentID, boundary_string);
	} else {
		sprintf(head, "--%s\n"
			      "Content-Type: application/json; "
			      "charset=UTF-8\n\n"
			      "{"
			      "\"title\":\"%s\""
			      "}\n\n"
			      "--%s\n\n",
			boundary_string, file_title, boundary_string);
	}

	sprintf(tail, "\n\n--%s--", boundary_string);
}

void _check_forbidden_reason(FILE *body_fptr)
{
	json_error_t jerror;
	json_t *json_data = NULL, *json_msg = NULL, *json_error_data;
	const char *msg = NULL;

	fseek(body_fptr, 0, SEEK_SET);

	json_data =
	    json_loadf(body_fptr, JSON_DISABLE_EOF_CHECK, &jerror);
	if (!json_data) {
		write_log(0, "Error: Fail to read json file. In %s\n",
			  __func__);
		return;
	}
	json_error_data = json_object_get(json_data, "error");
	if (!json_error_data) {
		write_log(
		    0,
		    "Error: Fail to parse json file. Error data not found\n");
		goto out;
	}
	json_msg = json_object_get(json_error_data, "message");
	if (!json_msg) {
		write_log(0, "Error: Fail to parse json file. msg not found\n");
		goto out;
	}
	msg = json_string_value(json_msg);
	if (!msg) {
		write_log(0, "Error: Json file is corrupt. In %s\n", __func__);
		goto out;
	}

	/* Check */
	write_log(4, "Warn: Http 403 caused by '%s'", msg);

	if (strcmp(msg, "User Rate Limit Exceeded") == 0 ||
	    strcmp(msg, "Rate Limit Exceeded") == 0)
		goto out;

	/* Let conn be false */
	update_backend_status(FALSE, NULL);
out:
	json_delete(json_data);
	return;
}

int32_t hcfs_gdrive_get_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;

	FILE *gdrive_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, ret, errcode;
	int32_t num_retries;
	int64_t ret_pos;
	int64_t objsize;
	struct timeval stop, start, diff;
	double time_spent;
	int64_t xfer_thpt;
	BOOL fetch_quota;

	/* For GOOGLEDRIVE backend - token not set situation */
	if (googledrive_token[0] == 0)
		return 401;

	fetch_quota = strncmp("download_usermeta", curl_handle->id, 100) == 0
			  ? TRUE
			  : FALSE;
	if (obj_info && obj_info->fileID[0] == 0 && fetch_quota == FALSE) {
		write_log(4, "Required file id when get %s", objname);
		return -1;
	}

	sprintf(header_filename, "/dev/shm/googledrive_get_head%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	gdrive_header_fptr = fopen(header_filename, "w+");
	if (gdrive_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	setbuf(gdrive_header_fptr, NULL);

	chunk = NULL;
	chunk = curl_slist_append(chunk, googledrive_token);
	chunk = curl_slist_append(chunk, "Expect:");

	if (fetch_quota)
		ASPRINTF(&url, "https://www.googleapis.com/drive/v2/about");
	else
		ASPRINTF(
		    &url,
		    "https://www.googleapis.com/drive/v3/files/%s?alt=media",
		    obj_info->fileID);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, gdrive_header_fptr);

	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);

	TIMEIT(HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		goto errcode_handle;
	}

	curl_slist_free_all(chunk);
	chunk = NULL;

	ret_val = parse_http_header_retcode(gdrive_header_fptr);
	if (ret_val < 0) {
		char header[1024] = {0};

		fseek(gdrive_header_fptr, 0, SEEK_SET);
		fread(header, sizeof(char), 1000, gdrive_header_fptr);
		write_log(5, "Warn: Fail to parse %s header:\n%s", objname,
			  header);
		/* We still need to record this failure for xfer throughput */
		//change_xfer_meta(0, 0, 0, 1);
		goto errcode_handle;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	/* get object meta data */
	if (http_is_success(ret_val)) {
		char header[1024] = {0};

		FSEEK(gdrive_header_fptr, 0, SEEK_SET);
		fread(header, sizeof(char), 1000, gdrive_header_fptr);
		write_log(10, "download object %s header:\n%s", objname,
			  header);
		/* Record xfer throughput */
		FSEEK(fptr, 0, SEEK_END);
		FTELL(fptr);
		objsize = ret_pos;
		FSEEK(fptr, 0, SEEK_SET);
		COMPUTE_THROUGHPUT();
		/* Update xfer statistics if successful */
		change_xfer_meta(0, objsize, xfer_thpt, 1);
		write_log(
		    10, "Download obj %s, size %"PRId64", in %f seconds, %"PRId64" KB/s\n",
		    objname, objsize, time_spent, xfer_thpt);
	} else {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
	}

	fclose(gdrive_header_fptr);
	gdrive_header_fptr = NULL;
	UNLINK(header_filename);
	return ret_val;

errcode_handle:
	if (gdrive_header_fptr) {
		fclose(gdrive_header_fptr);
		unlink(header_filename);
	}
	if (chunk)
		curl_slist_free_all(chunk);

	return -1;
}

int32_t hcfs_gdrive_delete_object(char *objname,
				  CURL_HANDLE *curl_handle,
				  GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;
	char delete_command[10];
	FILE *gdrive_header_fptr, *gdrive_body_fptr;
	CURL *curl;
	char header_filename[200], body_filename[200];
	int32_t ret_val, errcode, ret;
	int32_t num_retries;

	UNUSED(objname);
	/* For GOOGLEDRIVE backend - token not set situation */
	if (googledrive_token[0] == 0)
		return 401;

	if (obj_info->fileID[0] == 0) {
		write_log(4, "Required file id when delete %s", objname);
		return -1;
	}

	sprintf(header_filename, "/dev/shm/googledrive_delete_head%s.tmp",
		curl_handle->id);
	sprintf(body_filename, "/dev/shm/googledrive_delete_body%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	gdrive_header_fptr = fopen(header_filename, "w+");
	if (gdrive_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	gdrive_body_fptr = fopen(body_filename, "w+");
	if (gdrive_body_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		fclose(gdrive_header_fptr);
		unlink(header_filename);
		return -1;
	}
	setbuf(gdrive_body_fptr, NULL);
	setbuf(gdrive_header_fptr, NULL);

	strcpy(delete_command, "DELETE");

	chunk = NULL;
	chunk = curl_slist_append(chunk, googledrive_token);
	chunk = curl_slist_append(chunk, "Expect:");

	ASPRINTF(&url, "https://www.googleapis.com/drive/v2/files/%s",
		 obj_info->fileID);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, gdrive_header_fptr);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, delete_command);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, gdrive_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);
	//curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

	HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(gdrive_header_fptr);
		fclose(gdrive_body_fptr);
		unlink(header_filename);
		unlink(body_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(gdrive_header_fptr);
	if (ret_val < 0) {
		fclose(gdrive_header_fptr);
		fclose(gdrive_body_fptr);
		unlink(header_filename);
		unlink(body_filename);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 402))
		update_backend_status(FALSE, NULL);
	else if (ret_val == 403)
		_check_forbidden_reason(gdrive_body_fptr);

	fclose(gdrive_header_fptr);
	fclose(gdrive_body_fptr);
	UNLINK(header_filename);
	UNLINK(body_filename);

	return ret_val;
errcode_handle:
	return -1;
}

int32_t hcfs_gdrive_put_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	struct curl_slist *chunk = NULL;
	int64_t objsize;
	object_put_control put_control;
	CURLcode res;
	char *url = NULL;
	FILE *gdrive_header_fptr, *gdrive_body_fptr;
	CURL *curl;
	char header_filename[100], body_filename[100];
	int32_t ret_val, ret, errcode;
	int32_t num_retries;
	int64_t ret_pos;
	struct timeval stop, start, diff;
	double time_spent;
	int64_t xfer_thpt;

	/* For GOOGLEDRIVE backend - token not set situation */
	if (googledrive_token[0] == 0)
		return 401;

	if (obj_info->fileID[0] == 0) {
		write_log(4, "Required file id when put %s", objname);
		return -1;
	}

	sprintf(header_filename, "/dev/shm/googledrive_put_head%s.tmp",
		curl_handle->id);
	sprintf(body_filename, "/dev/shm/googledrive_put_body%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	gdrive_header_fptr = fopen(header_filename, "w+");
	if (gdrive_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	gdrive_body_fptr = fopen(body_filename, "w+");
	if (gdrive_body_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		fclose(gdrive_header_fptr);
		return -1;
	}
	setbuf(gdrive_header_fptr, NULL);

	chunk = NULL;
	chunk = curl_slist_append(chunk, googledrive_token);
	chunk = curl_slist_append(chunk, "Expect:");

	FSEEK(fptr, 0, SEEK_END);
	FTELL(fptr);
	objsize = ret_pos;
	FSEEK(fptr, 0, SEEK_SET);
	/* write_log(10, "object size: %d, objname: %s\n", objsize, objname); */

	if (objsize < 0) {
		fclose(gdrive_header_fptr);
		unlink(header_filename);
		fclose(gdrive_body_fptr);
		unlink(body_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

	put_control.fptr = fptr;
	put_control.object_size = objsize;
	put_control.remaining_size = objsize;

	ASPRINTF(&url, "https://www.googleapis.com/upload/drive/v2/files/"
		       "%s?uploadType=media",
		 obj_info->fileID);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, gdrive_header_fptr);

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_PUT, 1L);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, objsize);
	curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&put_control);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, gdrive_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);

	TIMEIT(HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		goto errcode_handle;
	}

	curl_slist_free_all(chunk);
	chunk = NULL;
	ret_val = parse_http_header_retcode(gdrive_header_fptr);
	if (ret_val < 0) {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		goto errcode_handle;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 402)) {
		update_backend_status(FALSE, NULL);
	} else if (ret_val == 403) {
		_check_forbidden_reason(gdrive_body_fptr);
		goto errcode_handle;
	}

	fclose(gdrive_header_fptr);
	gdrive_header_fptr = NULL;
	UNLINK(header_filename);
	fclose(gdrive_body_fptr);
	gdrive_body_fptr = NULL;
	UNLINK(body_filename);

	if (http_is_success(ret_val)) {
		/* Record xfer throughput */
		COMPUTE_THROUGHPUT();
		/* Update xfer statistics if successful */
		change_xfer_meta(objsize, 0, xfer_thpt, 1);
		write_log(10, "Upload obj %s, size %" PRId64
			      ", in %f seconds, %" PRId64 " KB/s\n",
			  objname, objsize, time_spent, xfer_thpt);
	} else {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
	}

	return ret_val;

errcode_handle:
	if (gdrive_header_fptr) {
		fclose(gdrive_header_fptr);
		unlink(header_filename);
	}
	if (gdrive_body_fptr) {
		fclose(gdrive_body_fptr);
		unlink(body_filename);
	}
	if (chunk)
		curl_slist_free_all(chunk);

	return -1;
}

int32_t hcfs_gdrive_post_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	struct curl_slist *chunk = NULL;
	int64_t objsize;
	object_post_control post_control;
	CURLcode res;
	char *url = NULL;
	FILE *gdrive_header_fptr, *gdrive_body_fptr;
	CURL *curl;
	char header_filename[100], body_filename[100];
	int32_t ret_val, ret, errcode;
	int32_t num_retries;
	int64_t ret_pos;
	struct timeval stop, start, diff;
	double time_spent;
	int64_t xfer_thpt;
	int64_t total_size;
	char head[1024], tail[128], header_content_type[256];
	json_error_t jerror;
	json_t *json_data, *json_id;
	const char *id;

	/* For google drive backend - token not set situation */
	if (googledrive_token[0] == 0)
		return 401;
	if (obj_info->file_title[0] == 0) {
		write_log(4, "Required file id and file title when get %s",
			  objname);
		return -1;
	}

	sprintf(header_filename, "/dev/shm/googledrivehead%s.tmp",
		curl_handle->id);
	sprintf(body_filename, "/dev/shm/googledrive_body%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	gdrive_header_fptr = fopen(header_filename, "w+");
	if (gdrive_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	setbuf(gdrive_header_fptr, NULL);

	gdrive_body_fptr = fopen(body_filename, "w+");
	if (gdrive_body_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		fclose(gdrive_header_fptr);
		return -1;
	}
	setbuf(gdrive_header_fptr, NULL);
	setbuf(gdrive_body_fptr, NULL);

	if (obj_info->type == GDRIVE_FOLDER) {
		chunk = NULL;
		chunk = curl_slist_append(chunk, "Expect:");
		chunk = curl_slist_append(chunk, googledrive_token);
		chunk =
		    curl_slist_append(chunk, "Content-Type: application/json");
		_create_folder_json_content(head, tail, obj_info->file_title,
					    obj_info->parentID);
		ASPRINTF(&url,
			 "https://www.googleapis.com/drive/v2/files");
	} else {
		char boundary_string[40];

		get_random_string(boundary_string, 32);
		sprintf(header_content_type,
			"Content-Type:multipart/related; boundary=%s",
			boundary_string);
		chunk = NULL;
		chunk = curl_slist_append(chunk, "Expect:");
		chunk = curl_slist_append(chunk, googledrive_token);
		chunk = curl_slist_append(chunk, header_content_type);
		_create_multipart_data(head, tail, obj_info->file_title,
				       obj_info->parentID, boundary_string);
		ASPRINTF(&url, "https://www.googleapis.com/upload/drive/v2/"
			       "files?uploadType=multipart");
	}

	/* Fetch object size */
	if (fptr) {
		FSEEK(fptr, 0, SEEK_END);
		FTELL(fptr);
		objsize = ret_pos;
		FSEEK(fptr, 0, SEEK_SET);
	} else {
		objsize = 0;
	}
	/* write_log(10, "object size: %d, objname: %s\n", objsize,
	 * objname); */

	if (objsize < 0) {
		write_log(0, "Object %s size smaller than zero.", objname);
		goto errcode_handle;
	}

	post_control.fptr = fptr;
	post_control.object_size = objsize;
	post_control.object_remaining_size = objsize;
	post_control.head_string = head;
	post_control.head_remaining = strlen(head);
	post_control.tail_string = tail;
	post_control.tail_remaining = strlen(tail);
	total_size =
	    objsize + post_control.head_remaining + post_control.tail_remaining;
	post_control.total_remaining = total_size;

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, gdrive_header_fptr);

	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, total_size);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, total_size);
	curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&post_control);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_post_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, gdrive_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);

	TIMEIT(HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		goto errcode_handle;
	}

	curl_slist_free_all(chunk);
	chunk = NULL;
	ret_val = parse_http_header_retcode(gdrive_header_fptr);
	if (ret_val < 0) {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		goto errcode_handle;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 402)) {
		update_backend_status(FALSE, NULL);
	} else if (ret_val == 403) {
		_check_forbidden_reason(gdrive_body_fptr);
		goto errcode_handle;
	}

	fclose(gdrive_header_fptr);
	gdrive_header_fptr = NULL;
	UNLINK(header_filename);

	/* Read returned content and fetch id */
	fseek(gdrive_body_fptr, 0, SEEK_SET);
	json_data =
	    json_loadf(gdrive_body_fptr, JSON_DISABLE_EOF_CHECK, &jerror);
	if (!json_data) {
		write_log(0, "Error: Fail to read json file. Error %s. Code %d",
			  jerror.text, ret_val);
		goto errcode_handle;
	}
	json_id = json_object_get(json_data, "id");
	if (!json_id) {
		json_delete(json_data);
		write_log(0, "Error: Fail to parse json file. id not found\n");
		goto errcode_handle;
	}
	id = json_string_value(json_id);
	if (!id) {
		json_delete(json_data);
		write_log(0, "Error: Json file is corrupt\n");
		goto errcode_handle;
	}
	strcpy(obj_info->fileID, id);
	fclose(gdrive_body_fptr);
	unlink(body_filename);
	json_delete(json_data);

	if (http_is_success(ret_val)) {
		/* Record xfer throughput */
		COMPUTE_THROUGHPUT();
		/* Update xfer statistics if successful */
		change_xfer_meta(objsize, 0, xfer_thpt, 1);
		write_log(10, "Upload obj %s, size %" PRId64
			      ", in %f seconds, %" PRId64 " KB/s\n",
			  objname, objsize, time_spent, xfer_thpt);
	} else {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
	}

	return ret_val;

errcode_handle:
	if (gdrive_header_fptr) {
		fclose(gdrive_header_fptr);
		unlink(header_filename);
	}
	if (gdrive_body_fptr) {
		fclose(gdrive_body_fptr);
		//TODO: unlink(body_filename);
	}
	if (chunk)
		curl_slist_free_all(chunk);
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
	} else {
		filter_string = title_string;
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
	update_backend_status((res == CURLE_OK), NULL);
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
		update_backend_status(FALSE, NULL);
	if (!http_is_success(ret_val))
		ftruncate(fileno(fptr), 0);

	fclose(gdrive_header_fptr);
	gdrive_header_fptr = NULL;
	UNLINK(header_filename);
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

int32_t get_parent_id(char *id, const char *objname)
{
	char hcfs_root_id_filename[200];
	CURL_HANDLE upload_handle;
	GOOGLEDRIVE_OBJ_INFO gdrive_info;
	int32_t ret = 0;
	FILE *fptr;

	memset(&upload_handle, 0, sizeof(CURL_HANDLE));

	/* Only data and meta are put under hcfs_root_folder */
	if (strncmp(objname, "data", 4) && strncmp(objname, "meta", 4)) {
		id[0] = 0;
		goto out;
	}

	if (gdrive_folder_id_cache->hcfs_folder_id[0]) {
		strncpy(id, gdrive_folder_id_cache->hcfs_folder_id,
			GDRIVE_ID_LENGTH);
		write_log(6, "parent id is %s", id);
		goto out;
	}

	snprintf(hcfs_root_id_filename, sizeof(hcfs_root_id_filename),
		 "%s/hcfs_root_id", METAPATH);

	/* Create folder */
	sem_wait(&(gdrive_folder_id_cache->op_lock));
	if (gdrive_folder_id_cache->hcfs_folder_id[0]) {
		strncpy(id, gdrive_folder_id_cache->hcfs_folder_id,
			GDRIVE_ID_LENGTH);
		sem_post(&(gdrive_folder_id_cache->op_lock));
		goto out;
	}

	fptr = fopen(hcfs_root_id_filename, "r");
	if (fptr) { /* Read id and fetch to mem cache */
		int64_t id_len;
		int64_t ret_size;
		fseek(fptr, 0, SEEK_END);
		id_len = ftell(fptr);
		fseek(fptr, 0, SEEK_SET);
		ret_size = fread(id, id_len, 1, fptr);
		if (ret_size == 1) {
			id[id_len] = 0;
			strncpy(gdrive_folder_id_cache->hcfs_folder_id, id,
				GDRIVE_ID_LENGTH);
			sem_post(&(gdrive_folder_id_cache->op_lock));
			goto out;
		} else {
			fclose(fptr);
		}
	} else {
		if (errno != ENOENT) {
			ret = -errno;
			sem_post(&(gdrive_folder_id_cache->op_lock));
			goto out;
		}
	}

	/* Create folder */
	memset(&gdrive_info, 0, sizeof(GOOGLEDRIVE_OBJ_INFO));
	strcpy(gdrive_info.file_title, "hcfs_data_folder");
	gdrive_info.type = GDRIVE_FOLDER;

	snprintf(upload_handle.id, sizeof(upload_handle.id),
		 "create_folder_curl");
	upload_handle.curl_backend = NONE;
	upload_handle.curl = NULL;
	ret = hcfs_put_object(NULL, "hcfs_data_folder", &upload_handle, NULL,
			      &gdrive_info);
	if ((ret < 200) || (ret > 299)) {
		ret = -EIO;
		write_log(0, "Error in creating hcfs_data_folder\n");
		sem_post(&(gdrive_folder_id_cache->op_lock));
		goto out;
	}

	fptr = fopen(hcfs_root_id_filename, "w+");
	if (!fptr) {
		ret = -errno;
		sem_post(&(gdrive_folder_id_cache->op_lock));
		goto out;
	}
	fwrite(gdrive_info.fileID, strlen(gdrive_info.fileID), 1, fptr);
	fclose(fptr);
	/* Copy to cache */
	strncpy(gdrive_folder_id_cache->hcfs_folder_id, gdrive_info.fileID,
		GDRIVE_ID_LENGTH);
	/* Copy to target */
	strncpy(id, gdrive_folder_id_cache->hcfs_folder_id, GDRIVE_ID_LENGTH);
	sem_post(&(gdrive_folder_id_cache->op_lock));

out:
	if (upload_handle.curl != NULL)
		hcfs_destroy_backend(&upload_handle);
	if (ret < 0)
		write_log(0, "Error in fetching parent id. Code %d", -ret);
	return ret;
}

/*
__attribute__((constructor))
void test()
{
	CURL_HANDLE curl_handle;
	//GOOGLEDRIVE_OBJ_INFO info;
	//FILE *fptr;
	int32_t ret;

	curl_handle.curl = curl_easy_init();
	curl_handle.curl_backend = GOOGLEDRIVE;
	strcpy(curl_handle.id, "test");
	strcpy(googledrive_token, "Authorization:Bearer ya29.CjDPAyAA2KGRGsp7Wd6TJ6rzo7HAcp4BRXevOENeJnKcKZGxJmIzpKcnoDugnQEplB4");

	info.fileID[0] = 0;
	info.parentID[0] = 0;
	//strcpy(info.file_title, "test.jpg");
	//strcpy(info.parentID, "0B9tmNFTdZf0beEo0MW5MQXNtTXM");
	strcpy(info.fileID, "0B9tmNFTdZf0bWnZMa3N2T2ZvZWM");

	fptr = fopen("testfile.jpg", "r");
	int32_t ret = hcfs_gdrive_put_object(fptr, "test.jpg", &curl_handle, &info);
	printf("ret = %d, %s\n", ret, info.fileID);
	fclose(fptr);
	
	printf("-----download test-----\n");
	if (!(fptr = fopen("testdownload.jpg", "w+")))
		exit(-1);	
	ret = hcfs_gdrive_get_object(fptr, "test.jpg", &curl_handle, &info);
	printf("ret = %d, %s", ret, info.fileID);
	fclose(fptr);

	printf("-----delete test-----\n");
	ret = hcfs_gdrive_delete_object("test.jpg", &curl_handle, &info);
	printf("ret = %d, %s", ret, info.fileID);
	ret = hcfs_gdrive_test_backend(&curl_handle);
	printf("ret = %d", ret);
	exit(0);
}
*/

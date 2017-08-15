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

#define MAX_BACKOFF_EXP 3

void gdrive_exp_backoff_sleep(int32_t busy_retry_times)
{
	struct timeval current_time;
	struct timespec sleep_time;
	int32_t n, max, min;

	if (busy_retry_times <= 0) {
		write_log(4, "Retry times LE than 0.");
		goto out;
	}

	gettimeofday(&current_time, NULL);
	srandom((uint32_t)(current_time.tv_usec));

	n = busy_retry_times > MAX_BACKOFF_EXP ? MAX_BACKOFF_EXP
					       : busy_retry_times;
	max = (1 << n) + 1;
	min = 1 << (n - 1);

	/* sleep in range (2^(n-1), 2^n + 1), that is:
	 * (1, 3), (2, 5), (4, 9)... */
	sleep_time.tv_sec = min + (random() % (max - min));
	sleep_time.tv_nsec = random() % 999999999;
	nanosleep(&sleep_time, NULL);

out:
	return;
}

size_t read_post_file_function(void *ptr, size_t size, size_t nmemb,
			  void *post_control1)
{
	FILE *fptr;
	size_t actual_to_read;
	size_t ret_size;
	size_t expect_read, total_read = 0;
	object_post_control *post_control;

	post_control = (object_post_control *)post_control1;

	total_read = 0;
	if (post_control->total_remaining <= 0)
		goto complete;

	expect_read = size * nmemb;
	if (expect_read <= 0)
		goto complete;

	/* Read head */
	if (post_control->head_remaining > 0) {
		if (expect_read > (size_t)post_control->head_remaining)
			actual_to_read = post_control->head_remaining;
		else
			actual_to_read = expect_read;
		memcpy((char *)ptr, post_control->head_string, actual_to_read);
		post_control->head_string += actual_to_read; /* Move pointer */
		post_control->head_remaining -= actual_to_read;
		post_control->total_remaining -= actual_to_read;
		expect_read -= actual_to_read;
		total_read += actual_to_read;
		if (expect_read <= 0)
			goto complete;
	}

	/* Body */
	if (post_control->object_remaining_size > 0 &&
	    post_control->fptr != NULL) {
		fptr = post_control->fptr;
		if (expect_read > (size_t)post_control->object_remaining_size)
			actual_to_read = post_control->object_remaining_size;
		else
			actual_to_read = expect_read;
		ret_size = FREAD((char *)ptr + total_read, 1, actual_to_read, fptr);
		post_control->object_remaining_size -= ret_size;
		post_control->total_remaining -= ret_size;
		expect_read -= ret_size;
		total_read += ret_size;
		if (expect_read <= 0)
			goto complete;
	}

	/* Tail */
	if (post_control->tail_remaining > 0) {
		if (expect_read > (size_t)post_control->tail_remaining)
			actual_to_read = post_control->tail_remaining;
		else
			actual_to_read = expect_read;
		memcpy((char *)ptr + total_read, post_control->tail_string,
				actual_to_read);
		post_control->tail_string += actual_to_read; /* Move pointer */
		post_control->tail_remaining -= actual_to_read;
		post_control->total_remaining -= actual_to_read;
		total_read += actual_to_read;
	}

complete:
	//if (strncmp(post_control->objname, "data_4600", 9) == 0)
	//	fwrite(ptr, 1, actual_to_read, stdout);
	return total_read;

errcode_handle:
	errno = errcode;
	return 0;
}

int32_t init_gdrive_token_control(void)
{
	int32_t ret = 0;

	if (googledrive_token_control)
		goto out;

	/* Init folder id cache */
	gdrive_folder_id_cache = (GOOGLE_DRIVE_FOLDER_ID_CACHE_T *)calloc(
	    sizeof(GOOGLE_DRIVE_FOLDER_ID_CACHE_T), 1);
	if (!gdrive_folder_id_cache) {
		ret = -errno;
		goto out;
	}
	sem_init(&(gdrive_folder_id_cache->op_lock), 0, 1);

	/* Init google drive token controller */
	googledrive_token_control =
	    (BACKEND_TOKEN_CONTROL *)calloc(sizeof(BACKEND_TOKEN_CONTROL), 1);
	if (!googledrive_token_control) {
		FREE(gdrive_folder_id_cache);
		ret = -errno;
		goto out;
	}

	pthread_mutex_init(&googledrive_token_control->access_lock, NULL);
	pthread_mutex_init(&googledrive_token_control->waiting_lock, NULL);
	pthread_cond_init(&googledrive_token_control->waiting_cond, NULL);

out:
	return ret;
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
		if (res != CURLE_ABORTED_BY_CALLBACK)
			write_log(4, "Curl op failed %s\n",
			          curl_easy_strerror(res));

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

int32_t _check_forbidden_reason(FILE *body_fptr)
{
	json_error_t jerror;
	json_t *json_data = NULL, *json_msg = NULL, *json_error_data;
	const char *msg = NULL;
	int32_t ret = 403;

	fseek(body_fptr, 0, SEEK_SET);

	json_data =
	    json_loadf(body_fptr, JSON_DISABLE_EOF_CHECK, &jerror);
	if (!json_data) {
		write_log(0, "Error: Fail to read json file. In %s\n",
			  __func__);
		return 403;
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

	if (strcasecmp(msg, "User Rate Limit Exceeded") == 0 ||
	    strcasecmp(msg, "Rate Limit Exceeded") == 0) {
		ret = -EBUSY;
		goto out;
	}

	/* Let conn be false */
	write_log(4, "Warn: Http 403 caused by '%s'", msg);
	update_backend_status(FALSE, NULL);
	ret = 403;
out:
	json_delete(json_data);
	return ret;
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
	int32_t ret_val, errcode;
	int64_t ret_pos;
	int64_t objsize;
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

	time_spent = TIMEIT(res = HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		if (res != CURLE_ABORTED_BY_CALLBACK)
			write_log(4, "Curl op failed %s\n",
			          curl_easy_strerror(res));
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
	    (ret_val >= 400 && ret_val <= 402))
		update_backend_status(FALSE, NULL);
	else if (ret_val == 403)
		ret_val = _check_forbidden_reason(fptr);

	/* get object meta data */
	if (http_is_success(ret_val)) {
		char header[1024] = {0};

		FSEEK(gdrive_header_fptr, 0, SEEK_SET);
		fread(header, sizeof(char), 1000, gdrive_header_fptr);
		write_log(10, "download object %s header:\n%s", objname,
			  header);
		/* Record xfer throughput */
		FSEEK(fptr, 0, SEEK_END);
		ret_pos = FTELL(fptr);
		objsize = ret_pos;
		FSEEK(fptr, 0, SEEK_SET);
		COMPUTE_THROUGHPUT(&xfer_thpt, &time_spent, objsize);
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
	int32_t ret_val, errcode;

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

	res = HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		if (res != CURLE_ABORTED_BY_CALLBACK)
			write_log(4, "Curl op failed %s\n",
			          curl_easy_strerror(res));
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
		ret_val = _check_forbidden_reason(gdrive_body_fptr);

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
	int32_t ret_val, errcode;
	int64_t ret_pos;
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
	ret_pos = FTELL(fptr);
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

	time_spent = TIMEIT(res = HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		if (res != CURLE_ABORTED_BY_CALLBACK)
			write_log(4, "Curl op failed %s\n",
			          curl_easy_strerror(res));
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
		ret_val = _check_forbidden_reason(gdrive_body_fptr);
	}

	fclose(gdrive_header_fptr);
	gdrive_header_fptr = NULL;
	UNLINK(header_filename);
	fclose(gdrive_body_fptr);
	gdrive_body_fptr = NULL;
	UNLINK(body_filename);

	if (http_is_success(ret_val)) {
		/* Record xfer throughput */
		COMPUTE_THROUGHPUT(&xfer_thpt, &time_spent, objsize);
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
	int32_t ret_val, errcode;
	int64_t ret_pos;
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
		ret_pos = FTELL(fptr);
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
	post_control.objname = obj_info->file_title;

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

	time_spent = TIMEIT(res = HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		if (res != CURLE_ABORTED_BY_CALLBACK)
			write_log(4, "Curl op failed %s\n",
			          curl_easy_strerror(res));
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
		ret_val = _check_forbidden_reason(gdrive_body_fptr);
		goto set_retval_busy_handle;
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
		COMPUTE_THROUGHPUT(&xfer_thpt, &time_spent, objsize);
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
	ret_val = -1;
set_retval_busy_handle:
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
	return ret_val;
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
	int32_t ret_val, errcode;
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
		title_string = NULL;
	}

	/* Create URL */
	if (filter_string) {
		write_log(0, "TEST: %s\n", filter_string);
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

	res = HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);
	curl_slist_free_all(chunk);

	if (res != CURLE_OK) {
		if (res != CURLE_ABORTED_BY_CALLBACK)
			write_log(4, "Curl op failed %s\n",
			          curl_easy_strerror(res));
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
	FREE(url);
	if (gdrive_header_fptr != NULL) {
		fclose(gdrive_header_fptr);
		unlink(header_filename);
		/* curl_slist_free_all(chunk); */
	}

	return -1;
}

void hcfs_destroy_gdrive_backend(CURL *curl) { curl_easy_cleanup(curl); }

int32_t fetch_id_from_list_body(FILE *fptr, char *id, const char *objname)
{
	json_error_t jerror;
	json_t *json_data, *json_items, *json_title, *json_id;
	const char *temp_id = NULL, *title = NULL;
	char buffer[8192] = {0};

	fseek(fptr, 0, SEEK_SET);
	json_data =
	    json_loadf(fptr, JSON_DISABLE_EOF_CHECK, &jerror);
	if (!json_data) {
		write_log(0, "Error: Fail to read json file. Error %s.",
			  jerror.text);
		goto errcode_handle;
	}
	/* Get list with key "items" */
	json_items = json_object_get(json_data, "items");
	if (!json_items) {
		write_log(0,
			  "Error: Fail to parse json file. items not found\n");
		goto errcode_handle_json_del;
	}

	/* Fetch "id" of given file title "objname" */
	size_t index;
	json_t *json_value;
	bool found = false;

	json_array_foreach(json_items, index, json_value) {
		/* Skip if not a json dict */
		if (json_is_object(json_value) < 0)
			continue;

		json_title = json_object_get(json_value, "title");
		if (!json_title) {
			write_log(0, "Error: Fail to parse json file. title "
				     "not found\n");
			goto errcode_handle_json_del;
		}
		title = json_string_value(json_title);
		if (!title) {
			write_log(0, "Error: Json file is corrupt\n");
			goto errcode_handle_json_del;
		}
		/* Fetch id if title matches objname */
		if (!strcmp(title, objname)) {
			found = true;
			json_id = json_object_get(json_value, "id");
			if (!json_id) {
				write_log(0, "Error: Fail to parse json file. "
					     "id not found\n");
				goto errcode_handle_json_del;
			}
			temp_id = json_string_value(json_id);
			if (!temp_id) {
				write_log(0, "Error: Json file is corrupt\n");
				goto errcode_handle_json_del;
			}
			strcpy(id, temp_id);
			break;
		}
	}
	json_delete(json_data);
	if (found == false) {
		write_log(0, "Object %s is not found.", objname);
		goto errcode_handle;
	}

	fseek(fptr, 0, SEEK_SET);
	fread(buffer, 8190, 1, fptr);
	write_log(0, "success to parse root folder id. Dump content:\n %s\n",
		  buffer);
	return 0;

errcode_handle_json_del:
	json_delete(json_data);
errcode_handle:
	fseek(fptr, 0, SEEK_SET);
	fread(buffer, 8190, 1, fptr);
	write_log(0, "Fail to parse root folder id. Dump content:\n %s\n",
		  buffer);
	return -1;
}


/**
 * Get object parent ID
 *
 * Given objname, get parent id from cache if it exists. Otherwise create a tera
 * root folder and then fetch folder id.
 *
 * @param id Pointer of parent ID
 * @param objname Object name
 */
int32_t get_parent_id(char *id, const char *objname)
{
	char hcfs_root_id_filename[200];
	CURL_HANDLE root_handle;
	GOOGLEDRIVE_OBJ_INFO gdrive_info;
	int32_t ret = 0;
	FILE *fptr;

	write_log(0, "TEST: Enter in fetch parent id. obj %s", objname);
	memset(&root_handle, 0, sizeof(CURL_HANDLE));

	/* Now data/meta/fsmgr-backup are put under folder "teradata" */
	if (strncmp(objname, "data", 4) != 0 &&
	    strncmp(objname, "meta", 4) != 0 &&
	    strncmp(objname, "FSstat", 6) != 0 &&
	    strncmp(objname, "backup_pkg", 10) != 0 &&
	    strncmp(objname, FSMGR_BACKUP, strlen(FSMGR_BACKUP)) != 0) {
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
		goto unlock_sem_out;
	}

	fptr = fopen(hcfs_root_id_filename, "r");
	if (fptr) { /* Read id and fetch to mem cache */
		int64_t id_len;
		int64_t ret_size;
		fseek(fptr, 0, SEEK_END);
		id_len = ftell(fptr);
		fseek(fptr, 0, SEEK_SET);
		ret_size = fread(id, id_len, 1, fptr);
		if (ret_size == 1) { /* Read id sucessfully */
			id[id_len] = 0;
			strncpy(gdrive_folder_id_cache->hcfs_folder_id, id,
				GDRIVE_ID_LENGTH);
			goto unlock_sem_out;
		} else {
			fclose(fptr);
		}
	} else {
		if (errno != ENOENT) {
			ret = -errno;
			goto unlock_sem_out;
		}
	}

	/* Find the id of tera folder */
	if (hcfs_system->system_restoring == RESTORING_STAGE1 ||
	    hcfs_system->system_restoring == RESTORING_STAGE2) {
		FILE *fptr;
		char *list_path = "/tmp/list_root_folder_and_fetch_id";

		memset(&gdrive_info, 0, sizeof(GOOGLEDRIVE_OBJ_INFO));
		strcpy(gdrive_info.file_title, GOOGLEDRIVE_FOLDER_NAME);
		gdrive_info.type = GDRIVE_FOLDER;
		snprintf(root_handle.id, sizeof(root_handle.id),
			 "create_folder_curl");
		root_handle.curl_backend = CURRENT_BACKEND;
		root_handle.curl = NULL;

		fptr = fopen(list_path, "w+");
		if (!fptr) {
			ret = -errno;
			goto unlock_sem_out;
		}
		ret = hcfs_list_container(fptr, &root_handle, &gdrive_info);
		if ((ret < 200) || (ret > 299)) {
			write_log(0, "Error in list %s. Http code %d\n",
				  GOOGLEDRIVE_FOLDER_NAME, ret);
			ret = -EIO;
			fclose(fptr);
			goto unlock_sem_out;
		}
		ret =
		    fetch_id_from_list_body(fptr, id, GOOGLEDRIVE_FOLDER_NAME);
		if (ret < 0) {
			/* TODO: error handling when folder not found */
			ret = -EIO;
			fclose(fptr);
			goto unlock_sem_out;
		}
		unlink(list_path);

	} else {
		/* Create folder */
		memset(&gdrive_info, 0, sizeof(GOOGLEDRIVE_OBJ_INFO));
		strcpy(gdrive_info.file_title, GOOGLEDRIVE_FOLDER_NAME);
		gdrive_info.type = GDRIVE_FOLDER;

		snprintf(root_handle.id, sizeof(root_handle.id),
				"create_folder_curl");
		root_handle.curl_backend = NONE;
		root_handle.curl = NULL;
		ret = hcfs_put_object(NULL, GOOGLEDRIVE_FOLDER_NAME,
				      &root_handle, NULL, &gdrive_info);
		if ((ret < 200) || (ret > 299)) {
			ret = -EIO;
			write_log(0, "Error in creating %s\n",
				  GOOGLEDRIVE_FOLDER_NAME);
			goto unlock_sem_out;
		}
		strncpy(id, gdrive_info.fileID, GDRIVE_ID_LENGTH);
	}

	/* Write to file */
	fptr = fopen(hcfs_root_id_filename, "w+");
	if (!fptr) {
		ret = -errno;
		goto unlock_sem_out;
	}
	fwrite(id, strlen(id), 1, fptr);
	fclose(fptr);
	/* Copy to cache */
	strncpy(gdrive_folder_id_cache->hcfs_folder_id, id,
		GDRIVE_ID_LENGTH);

unlock_sem_out:
	sem_post(&(gdrive_folder_id_cache->op_lock));

out:
	if (root_handle.curl != NULL)
		hcfs_destroy_backend(&root_handle);
	if (ret < 0)
		write_log(0, "Error in fetching parent id. Code %d", -ret);
	return ret;
}

int32_t query_object_id(GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	CURL_HANDLE list_handle;
	char list_path[400] = {0};
	FILE *fptr;
	int32_t ret = 0;

	snprintf(list_handle.id, sizeof(list_handle.id), "list_object_%s",
		 obj_info->file_title);
	list_handle.curl_backend = CURRENT_BACKEND;
	list_handle.curl = NULL;

	snprintf(list_path, sizeof(list_path), "/tmp/list_obj_content_%s",
		 obj_info->file_title);
	fptr = fopen(list_path, "w+");
	if (!fptr) {
		ret = -errno;
		goto out;
	}
	write_log(0, "TEST: query file: %s, parent: %s", obj_info->file_title,
		  obj_info->parentID);
	ret = hcfs_list_container(fptr, &list_handle, obj_info);
	if ((ret < 200) || (ret > 299)) {
		write_log(0, "Error in list %s. Http code %d\n",
			  GOOGLEDRIVE_FOLDER_NAME, ret);
		ret = -EIO;
		fclose(fptr);
		goto out;
	}
	ret = fetch_id_from_list_body(fptr, obj_info->fileID,
				      obj_info->file_title);
	if (ret < 0) {
		fclose(fptr);
		goto out;
	}
	unlink(list_path);

out:
	if (list_handle.curl != NULL)
		hcfs_destroy_backend(&list_handle);
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

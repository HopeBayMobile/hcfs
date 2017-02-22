/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfscurl.c
* Abstract: The c source code file for CURL operations.
*
* Revision History
* 2015/2/17 Jiahong added header for this file, and revising coding style.
* 2015/6/3, 6/4 Jiahong added error handling
*
**************************************************************************/

/* Implemented routines to parse http return code to distinguish errors
 * from normal ops
 */

#include "hcfscurl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/hmac.h>
#include <openssl/engine.h>
#include <pthread.h>
#include <semaphore.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <errno.h>

#include "b64encode.h"
#include "params.h"
#include "logger.h"
#include "macro.h"
#include "global.h"
#include "fuseop.h"
#include "monitor.h"
#include "utils.h"
#include "event_notification.h"
#include "event_filter.h"
#include "googledrive_curl.h"

/* For SWIFTTOKEN backend only */
BACKEND_TOKEN_CONTROL swifttoken_control = {
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_MUTEX_INITIALIZER,
	PTHREAD_COND_INITIALIZER
};

char swift_auth_string[1024];
char swift_url_string[1024];


/************************************************************************
*
* Function name: write_file_fn
*        Inputs: void *ptr, size_t size, size_t nmemb, void *fstream
*       Summary: Same as fwrite but will return total bytes.
*  Return value: Total size for writing to fstream. Return 0 if error.
*
*************************************************************************/
size_t write_file_fn(void *ptr, size_t size, size_t nmemb, void *fstream)
{
	size_t ret_size;
	int32_t errcode = -1;

	FWRITE(ptr, size, nmemb, (FILE *)fstream);

	return ret_size * size;

errcode_handle:
	errno = errcode;
	return 0;
}

/************************************************************************
*
* Function name: parse_swift_auth_header
*        Inputs: FILE *fptr
*       Summary: Parse the HTTP header for auth requests and return HTTP
*                return code (in integer). If successful, auth string is
*                stored in the global variable "swift_auth_string".
*  Return value: Return code from HTTP header, or -1 if error.
*
*************************************************************************/
int32_t parse_swift_auth_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	char *endptr;
	char temp_string[1024], temp_string2[1024];
	int64_t ret_num;
	int32_t retcodenum, ret_val;
	int32_t ret, errcode;
	char to_stop;

	FSEEK(fptr, 0, SEEK_SET);
	ret_val = fscanf(fptr, "%19s %19s %19[^\r\n]\n", httpcode, retcode,
			 retstatus);
	if (ret_val < 3)
		return -1;

	ATOL(retcode);
	retcodenum = (int32_t)ret_num;

	if ((retcodenum < 200) || (retcodenum > 299))
		return retcodenum;

	to_stop = FALSE;

	while (to_stop == FALSE) {
		ret_val = fscanf(fptr, "%1023s %1023[^\r\n]\n", temp_string,
				 temp_string2);
		if (ret_val < 2)
			return -1;
		if (strcmp(temp_string, "X-Storage-Url:") == 0) {
			strcpy(swift_url_string, temp_string2);
			to_stop = TRUE;
		}
	}

	to_stop = FALSE;

	while (to_stop == FALSE) {
		ret_val = fscanf(fptr, "%1023s %1023[^\r\n]\n", temp_string,
				 temp_string2);
		if (ret_val < 2)
			return -1;
		if (strcmp(temp_string, "X-Auth-Token:") == 0)
			to_stop = TRUE;
	}

	sprintf(swift_auth_string, "%s %s", temp_string, temp_string2);

	write_log(10, "Header info: %s, %s\n", swift_url_string,
		  swift_auth_string);
	return retcodenum;

errcode_handle:
	return -1;
}

/************************************************************************
*
* Function name: parse_swift_list_header
*        Inputs: FILE *fptr
*       Summary: Parse the HTTP header for Swift list requests and return HTTP
*                return code (in integer).
*  Return value: Return code from HTTP header, or -1 if error.
*
*************************************************************************/
int32_t parse_swift_list_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	char temp_string[1024], temp_string2[1024];
	int32_t ret_val, retcodenum, total_objs;
	int32_t ret, errcode;
	int64_t ret_num;
	char *endptr, *tmpptr;
	const char HEADERSTR_OBJCOUNT[] = "X-Container-Object-Count";

	FSEEK(fptr, 0, SEEK_SET);
	ret_val = fscanf(fptr, "%19s %19s", httpcode, retcode);
	if (ret_val < 2)
		return -1;

	tmpptr = fgets(retstatus, 19, fptr);
	if (tmpptr == NULL) {
		write_log(0, "Error parsing in %s\n", __func__);
		return -1;
	}
	ATOL(retcode);
	retcodenum = (int32_t)ret_num;

	if ((retcodenum < 200) || (retcodenum > 299))
		return retcodenum;

	while (!feof(fptr)) {
		tmpptr = fgets(temp_string, 1000, fptr);
		if (tmpptr == NULL) {
			write_log(2, "Warning: Cannot parse num of objs\n");
			return retcodenum;
		}

		if (!strncmp(temp_string, HEADERSTR_OBJCOUNT,
			     sizeof(HEADERSTR_OBJCOUNT) - 1)) {
			ret_val = sscanf(temp_string,
					 "X-Container-Object-Count: %1023s\n",
					 temp_string2);
			if (ret_val != 1)
				return -1;
			ATOL(temp_string2);
			total_objs = (int32_t)ret_num;

			write_log(10, "total objects %d\n", total_objs);

			return retcodenum;
		}
		memset(temp_string, 0, 1000);
	}
	return -1;
errcode_handle:
	return -1;
}

/************************************************************************
*
* Function name: parse_S3_list_header
*        Inputs: FILE *fptr
*       Summary: Parse the HTTP header for S3 list requests and return HTTP
*                return code (in integer).
*  Return value: Return code from HTTP header, or -1 if error.
*
*************************************************************************/
int32_t parse_S3_list_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	int32_t ret, retcodenum;
	int64_t ret_num;
	int32_t errcode;
	char *endptr, *tmpptr;

	FSEEK(fptr, 0, SEEK_SET);
	ret = fscanf(fptr, "%19s %19s", httpcode, retcode);
	if (ret < 2)
		return -1;

	tmpptr = fgets(retstatus, 19, fptr);
	if (tmpptr == NULL) {
		write_log(0, "Error parsing in %s\n", __func__);
		return -1;
	}
	ATOL(retcode);
	retcodenum = (int32_t)ret_num;

	return retcodenum;
errcode_handle:
	return -1;
}

/************************************************************************
*
* Function name: parse_http_header_retcode
*        Inputs: FILE *fptr
*       Summary: Parse the HTTP header for general requests and return HTTP
*                return code (in integer).
*  Return value: Return code from HTTP header, or -1 if error.
*
*************************************************************************/
int32_t parse_http_header_retcode(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	int32_t ret, retcodenum;
	int64_t ret_num;
	int32_t errcode;
	char *endptr;

	FSEEK(fptr, 0, SEEK_SET);
	ret = fscanf(fptr, "%19s %19s %19s", httpcode, retcode, retstatus);
	if (ret < 3)
		return -1;

	ATOL(retcode);
	retcodenum = (int32_t)ret_num;

	return retcodenum;

errcode_handle:
	return -1;
}

/************************************************************************
 *
 * Function name: parse_http_header_coding_meta
 *        Inputs: HCFS_object_meta *object_meta
 *                char* httpheader,
 *                int32_t header_len
 *       Summary: Parse the HTTP header for general requests and return
 *                write to object_meta
 *  Return value: 0 for success otherwise indicates an error
 *
 *************************************************************************/
int32_t parse_http_header_coding_meta(HCFS_encode_object_meta *object_meta,
				  char *httpheader, const char *meta,
				  const char *comp_meta, const char *enc_meta,
				  const char *nonce_meta)
{
	const int32_t meta_len = strlen(meta);
	const int32_t comp_meta_len = strlen(comp_meta);
	const int32_t enc_meta_len = strlen(enc_meta);
	const int32_t nonce_meta_len = strlen(nonce_meta);

	char *s, *backup;

	s = strtok_r(httpheader, "\n", &backup);
	while (s) {
		char *s2;
		char *backup_s2;
		int32_t s2_len;
		int32_t len_diff;

		s2 = strtok_r(s, ":", &backup_s2);
		s2_len = strlen(s2);
		if (s2_len <= meta_len)
			goto cont;
		if (memcmp(s2, meta, meta_len) != 0)
			goto cont;

		len_diff = s2_len - meta_len;

		switch (len_diff) {
		case 3:
			if (memcmp(enc_meta, s2 + meta_len, enc_meta_len) ==
			    0) {
				s2 = strtok_r(NULL, ":", &backup_s2);
				object_meta->enc_alg = atoi(s2);
			}
			break;
		case 4:
			if (memcmp(comp_meta, s2 + meta_len, comp_meta_len) ==
			    0) {
				s2 = strtok_r(NULL, ":", &backup_s2);
				object_meta->comp_alg = atoi(s2);
			}
			break;
		case 5:
			if (memcmp(nonce_meta, s2 + meta_len, nonce_meta_len) ==
			    0) {
				s2 = strtok_r(NULL, ":", &backup_s2);
				if (s2 != NULL) {
					object_meta->len_enc_session_key =
					    strlen(s2);
					object_meta->enc_session_key = calloc(
					    object_meta->len_enc_session_key+10,
					    sizeof(char));
					memcpy(
					    object_meta->enc_session_key, s2,
					    object_meta->len_enc_session_key);
				}
			}
			break;
		default:
			break;
		}

cont:
		s = strtok_r(NULL, "\n", &backup);
	}
	return 0;
}

/************************************************************************
*
* Function name: dump_list_body
*        Inputs: FILE *fptr
*       Summary: For Swift list requests, dump the content of results.
*  Return value: None.
*
*************************************************************************/
void dump_list_body(FILE *fptr)
{
	char temp_string[1024];
	int32_t ret_val;

	fseek(fptr, 0, SEEK_SET);
	while (!feof(fptr)) {
		ret_val = fscanf(fptr, "%1023s\n", temp_string);
		if (ret_val < 1)
			break;
		write_log(10, "%s\n", temp_string);
	}
}

/************************************************************************
*
* Function name: dump_list_body
*        Inputs: FILE *fptr
*       Summary: For S3 list requests, dump to stdout the content of results.
*  Return value: None.
*
*************************************************************************/
void dump_S3_list_body(FILE *fptr)
{
	char temp_string[1024];
	int32_t ret_val;

	fseek(fptr, 0, SEEK_SET);
	while (!feof(fptr)) {
		ret_val = fread(temp_string, 1, 512, fptr);
		temp_string[ret_val] = 0;
		if (ret_val < 1)
			break;
		write_log(10, "%s", temp_string);
	}
}

/************************************************************************
*
* Function name: hcfs_get_auth_swift
*        Inputs: char *swift_user, char *swift_pass, char *swift_url,
*                CURL_HANDLE *curl_handle
*       Summary: Send Swift auth request, and return appropriate result code.
*  Return value: Return code from HTTP header, or -1 if error.
*
*************************************************************************/
int32_t hcfs_get_auth_swift(char *swift_user, char *swift_pass, char *swift_url,
			CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url;
	char user_string[1024];
	char pass_string[1024];
	int32_t ret_val;
	FILE *fptr;
	CURL *curl;
	char filename[100];
	int32_t errcode, ret;
	int32_t num_retries;

	sprintf(filename, "/dev/shm/swiftauth%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	fptr = fopen(filename, "w+");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO Error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	chunk = NULL;

	ASPRINTF(&url, "%s://%s/auth/v1.0", SWIFT_PROTOCOL, swift_url);

	sprintf(user_string, "X-Storage-User: %s", swift_user);
	sprintf(pass_string, "X-Storage-Pass: %s", swift_pass);
	chunk = curl_slist_append(chunk, user_string);
	chunk = curl_slist_append(chunk, pass_string);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, fptr);

	HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(fptr);
		unlink(filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	ret_val = parse_swift_auth_header(fptr);
	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	/*TODO: add retry routines somewhere for failed attempts*/

	if (ret_val < 0) {
		fclose(fptr);
		unlink(filename);
		curl_slist_free_all(chunk);
		return ret_val;
	}

	fclose(fptr);
	UNLINK(filename);

	curl_slist_free_all(chunk);

	write_log(10, "Ret code %d\n", ret_val);
	return ret_val;

errcode_handle:
	return -1;
}

/************************************************************************
*
* Function name: hcfs_get_auth_token
*        Inputs:
*       Summary: Send TOKEN_EXPIRED event to notify server to ask for new
*                token.
*  Return value: Return 200 if event is sent, or -1 if error.
*
*************************************************************************/
int32_t hcfs_get_auth_token(void)
{
	int32_t ret_code;
	struct timespec timeout;
	BACKEND_TOKEN_CONTROL *token_controller = NULL;

	/* Token is already set */
	if (swift_auth_string[0] != 0)
		return 200;

	switch (CURRENT_BACKEND) {
	case SWIFTTOKEN:
		token_controller = &swifttoken_control;
		break;
	case GOOGLEDRIVE:
		token_controller = googledrive_token_control;
		break;
	default:
		write_log(0, "Error: Invalid backend type");
		return -1;
	}

	ret_code = add_notify_event(TOKEN_EXPIRED, NULL, FALSE);

	/* add event was successful or event already sent */
	if ((ret_code == 0 || ret_code == 3) &&
			(hcfs_system->system_going_down == FALSE)) {
		/* Wait for new token being set */
		pthread_mutex_lock(&(token_controller->waiting_lock));
		clock_gettime(CLOCK_REALTIME, &timeout);
		timeout.tv_sec += MAX_WAIT_TIME;
		pthread_cond_timedwait(&(token_controller->waiting_cond),
				&(token_controller->waiting_lock),
				&timeout);
		pthread_mutex_unlock(&(token_controller->waiting_lock));
		/* If system is shutting down, do not attempt followup
		 * operations
		 */
		if (hcfs_system->system_going_down == TRUE)
			return -ESHUTDOWN;

		return 200;
	} else {
		return -1;
	}
}

/************************************************************************
*
* Function name: hcfs_init_swift_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Initialize Swift backend for the curl handle "curl_handel".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_init_swift_backend(CURL_HANDLE *curl_handle)
{
	char account_user_string[1000];
	int32_t ret_code;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		curl_handle->curl = curl_easy_init();

		if (curl_handle->curl) {
			sprintf(account_user_string, "%s:%s", SWIFT_ACCOUNT,
				SWIFT_USER);

			ret_code =
			    hcfs_get_auth_swift(account_user_string, SWIFT_PASS,
						SWIFT_URL, curl_handle);

			return ret_code;
		}
		break;
	case SWIFTTOKEN:
		curl_handle->curl = curl_easy_init();

		if (curl_handle->curl) {
			ret_code = hcfs_get_auth_token();
			if (ret_code == 200 || swift_auth_string[0] != 0)
				return 200;
			else
				return ret_code;
		}
		break;
	default:
		break;
	}
	return -1;
}

/************************************************************************
*
* Function name: hcfs_init_S3_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Initialize S3 backend for the curl handle "curl_handel".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*          Note: We do not need to do anything special other than curl init.
*
*************************************************************************/
int32_t hcfs_init_S3_backend(CURL_HANDLE *curl_handle)
{
	curl_handle->curl = curl_easy_init();

	if (curl_handle->curl)
		return 200;
	return -1;
}

/************************************************************************
*
* Function name: hcfs_swift_reauth
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Reset curl handle and request a new Swift auth string.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_swift_reauth(CURL_HANDLE *curl_handle)
{
	char account_user_string[1000];
	int32_t ret_code;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		if (curl_handle->curl != NULL)
			hcfs_destroy_swift_backend(curl_handle->curl);

		curl_handle->curl = curl_easy_init();

		if (curl_handle->curl) {
			sprintf(account_user_string, "%s:%s", SWIFT_ACCOUNT,
				SWIFT_USER);

			ret_code =
			    hcfs_get_auth_swift(account_user_string, SWIFT_PASS,
						SWIFT_URL, curl_handle);

			return ret_code;
		}
		return -1;
	case SWIFTTOKEN:
		if (curl_handle->curl != NULL)
			hcfs_destroy_swift_backend(curl_handle->curl);

		curl_handle->curl = curl_easy_init();

		if (swift_auth_string[0] != 0) {
			ret_code = pthread_mutex_trylock(
					&(swifttoken_control.access_lock));
			if (ret_code == 0) {
				memset(swift_url_string,
						0, sizeof(swift_url_string));
				memset(swift_auth_string,
						0, sizeof(swift_auth_string));
				pthread_mutex_unlock(
				    &(swifttoken_control.access_lock));
			}
		}

		if (curl_handle->curl)
			return hcfs_get_auth_token();

		return -1;
	default:
		return -1;
	}
}

/************************************************************************
*
* Function name: hcfs_S3_reauth
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Reset curl handle and init again for S3 backend.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_S3_reauth(CURL_HANDLE *curl_handle)
{

	if (curl_handle->curl != NULL)
		hcfs_destroy_S3_backend(curl_handle->curl);

	return hcfs_init_S3_backend(curl_handle);
}

/************************************************************************
*
* Function name: hcfs_destroy_swift_backend
*        Inputs: CURL *curl
*       Summary: Cleanup curl handle
*  Return value: None.
*
*************************************************************************/
void hcfs_destroy_swift_backend(CURL *curl) { curl_easy_cleanup(curl); }

/************************************************************************
*
* Function name: hcfs_destroy_S3_backend
*        Inputs: CURL *curl
*       Summary: Cleanup curl handle
*  Return value: None.
*
*************************************************************************/
void hcfs_destroy_S3_backend(CURL *curl) { curl_easy_cleanup(curl); }

/************************************************************************
*
* Function name: read_file_function
*        Inputs: void *ptr, size_t size, size_t nmemb, void *put_control1
*       Summary: In put operations, read data from file stream pointed in
*                "put_control1", update the remaining size after the read,
*                and return the number of bytes read in this function call.
*  Return value: Number of bytes read in this function call.
*
*************************************************************************/
size_t read_file_function(void *ptr, size_t size, size_t nmemb,
			  void *put_control1)
{
	/*TODO: Consider if it is possible for the actual file size to be
	 * smaller than object size due to truncating
	 */
	FILE *fptr;
	size_t actual_to_read;
	object_put_control *put_control;
	int32_t errcode;
	size_t ret_size;

	put_control = (object_put_control *)put_control1;

	if (put_control->remaining_size <= 0)
		return 0;

	fptr = put_control->fptr;
	if ((size * nmemb) > (size_t)put_control->remaining_size)
		actual_to_read = put_control->remaining_size;
	else
		actual_to_read = size * nmemb;

	FREAD(ptr, 1, actual_to_read, fptr);
	put_control->remaining_size -= ret_size;

	return ret_size;
errcode_handle:
	errno = errcode;
	return 0;
}

/************************************************************************
*
* Function name: hcfs_swift_test_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: For Swift backends, displays information of the account.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_swift_test_backend(CURL_HANDLE *curl_handle)
{
	/*TODO: How to actually export the list of objects to other functions*/
	struct curl_slist *chunk = NULL;
	CURLcode res;
	FILE *swift_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, errcode;

	/* For SWIFTTOKEN backend - token not set situation */
	if (swift_auth_string[0] == 0)
		return 401;

	snprintf(header_filename, sizeof(header_filename),
		 "/dev/shm/swiftaccounthead%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");
	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	chunk = NULL;
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, swift_url_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_TIMEOUT, MONITOR_TEST_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

	res = curl_easy_perform(curl);

	if (res == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ret_val);
	else
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));

	fclose(swift_header_fptr);
	unlink(header_filename);
	curl_slist_free_all(chunk);

	if (res == CURLE_OK)
		return ret_val;
	else
		return -1;
}

/************************************************************************
*
* Function name: hcfs_swift_list_container
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: For Swift backends, list the current container.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_swift_list_container(CURL_HANDLE *curl_handle)
{
	/*TODO: How to actually export the list of objects to other functions*/
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;
	FILE *swift_header_fptr, *swift_list_body_fptr;
	CURL *curl;
	char header_filename[100], body_filename[100];
	int32_t ret_val, ret, num_retries, errcode;

	/* For SWIFTTOKEN backend - token not set situation */
	if (swift_auth_string[0] == 0)
		return 401;

	sprintf(header_filename, "/dev/shm/swiftlisthead%s.tmp",
		curl_handle->id);
	sprintf(body_filename, "/dev/shm/swiftlistbody%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");
	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	swift_list_body_fptr = fopen(body_filename, "w+");
	if (swift_list_body_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		fclose(swift_header_fptr);
		return -1;
	}

	chunk = NULL;

	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, "Content-Length:");

	ASPRINTF(&url, "%s/%s", swift_url_string, SWIFT_CONTAINER);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, swift_list_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);

	HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(swift_header_fptr);
		unlink(header_filename);
		fclose(swift_list_body_fptr);
		unlink(body_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	ret_val = parse_swift_list_header(swift_header_fptr);
	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	if ((ret_val >= 200) && (ret_val < 300))
		dump_list_body(swift_list_body_fptr);
	/*TODO: add retry routines somewhere for failed attempts*/

	fclose(swift_header_fptr);
	unlink(header_filename);
	fclose(swift_list_body_fptr);
	unlink(body_filename);

	curl_slist_free_all(chunk);

errcode_handle:
	return ret_val;
}

/************************************************************************
*
* Function name: hcfs_swift_put_object
*        Inputs: FILE *fptr, char *objname, CURL_HANDLE *curl_handle
*       Summary: For Swift backends, put to object "objname" by reading
*                from opened file pointed by "fptr", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_swift_put_object(FILE *fptr,
			      char *objname,
			      CURL_HANDLE *curl_handle,
			      HTTP_meta *object_meta)
{
	struct curl_slist *chunk = NULL;
	off_t objsize;
	object_put_control put_control;
	CURLcode res;
	char *url = NULL;
	char object_string[150];
	FILE *swift_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, ret, errcode;
	int32_t num_retries;
	int64_t ret_pos;
	struct timeval stop, start, diff;
	double time_spent;
	int64_t xfer_thpt;

	/* For SWIFTTOKEN backend - token not set situation */
	if (swift_auth_string[0] == 0)
		return 401;

	sprintf(header_filename, "/dev/shm/swiftputhead%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");

	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	setbuf(swift_header_fptr, NULL);
	chunk = NULL;

	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");
	if (object_meta != NULL) {
		int32_t i;

		for (i = 0; i < object_meta->count; i++) {
			sprintf(object_string, "X-OBJECT-META-%s: %s",
				object_meta->data[2 * i],
				object_meta->data[2 * i + 1]);
			chunk = curl_slist_append(chunk, object_string);
		}
	}

	FSEEK(fptr, 0, SEEK_END);
	FTELL(fptr);
	objsize = ret_pos;
	FSEEK(fptr, 0, SEEK_SET);
	/* write_log(10, "object size: %d, objname: %s\n", objsize, objname); */

	if (objsize < 0) {
		fclose(swift_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

	put_control.fptr = fptr;
	put_control.object_size = objsize;
	put_control.remaining_size = objsize;

	ASPRINTF(&url, "%s/%s/%s", swift_url_string, SWIFT_CONTAINER, objname);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_PUT, 1L);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, objsize);
	curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&put_control);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);

	TIMEIT(HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(swift_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(swift_header_fptr);
	if (ret_val < 0) {
		fclose(swift_header_fptr);
		unlink(header_filename);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	fclose(swift_header_fptr);
	swift_header_fptr = NULL;
	UNLINK(header_filename);

	if (http_is_success(ret_val)) {
		/* Record xfer throughput */
		COMPUTE_THROUGHPUT();
		/* Update xfer statistics if successful */
		change_xfer_meta(objsize, 0, xfer_thpt, 1);
		write_log(10,
			  "Upload obj %s, size %llu, in %f seconds, %d KB/s\n",
			  objname, objsize, time_spent, xfer_thpt);
	} else {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
	}

	return ret_val;

errcode_handle:
	if (swift_header_fptr == NULL) {
		fclose(swift_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
	}

	return -1;
}

/************************************************************************
*
* Function name: hcfs_swift_get_object
*        Inputs: FILE *fptr, char *objname, CURL_HANDLE *curl_handle
*       Summary: For Swift backends, get object "objname" and write to
*                opened file pointed by "fptr", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_swift_get_object(FILE *fptr,
			      char *objname,
			      CURL_HANDLE *curl_handle,
			      HCFS_encode_object_meta *object_meta)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;

	FILE *swift_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, ret, errcode;
	int32_t num_retries;
	int64_t ret_pos;
	off_t objsize;
	struct timeval stop, start, diff;
	double time_spent;
	int64_t xfer_thpt;

	/* For SWIFTTOKEN backend - token not set situation */
	if (swift_auth_string[0] == 0)
		return 401;

	sprintf(header_filename, "/dev/shm/swiftgethead%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");
	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}
	setbuf(swift_header_fptr, NULL);

	chunk = NULL;
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");

	if (!strncmp("download_usermeta", curl_handle->id, 100))
		ASPRINTF(&url, "%s/%s_gateway_config/%s", swift_url_string,
			 SWIFT_USER, objname);
	else
		ASPRINTF(&url, "%s/%s/%s", swift_url_string, SWIFT_CONTAINER,
			 objname);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);

	TIMEIT(HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(swift_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(swift_header_fptr);
	if (ret_val < 0) {
		char header[1024] = {0};

		fseek(swift_header_fptr, 0, SEEK_SET);
		fread(header, sizeof(char), 1000, swift_header_fptr);
		write_log(5, "Warn: Fail to parse %s header:\n%s", objname,
			  header);
		fclose(swift_header_fptr);
		unlink(header_filename);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	/* get object meta data */
	if (http_is_success(ret_val) && object_meta) {
		char header[1024] = {0};

		FSEEK(swift_header_fptr, 0, SEEK_SET);
		fread(header, sizeof(char), 1000, swift_header_fptr);
		write_log(10, "download object %s header:\n%s", objname,
			  header);
		parse_http_header_coding_meta(object_meta, header,
					      "X-Object-Meta-", "Comp", "Enc",
					      "Nonce");

		/* Record xfer throughput */
		FSEEK(fptr, 0, SEEK_END);
		FTELL(fptr);
		objsize = ret_pos;
		FSEEK(fptr, 0, SEEK_SET);
		COMPUTE_THROUGHPUT();
		/* Update xfer statistics if successful */
		change_xfer_meta(0, objsize, xfer_thpt, 1);
		write_log(
		    10, "Download obj %s, size %llu, in %f seconds, %d KB/s\n",
		    objname, objsize, time_spent, xfer_thpt);
	} else {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
	}

	fclose(swift_header_fptr);
	swift_header_fptr = NULL;
	UNLINK(header_filename);

	return ret_val;

errcode_handle:
	if (swift_header_fptr != NULL) {
		fclose(swift_header_fptr);
		unlink(header_filename);
		/* curl_slist_free_all(chunk); */
	}

	return -1;
}

/************************************************************************
*
* Function name: hcfs_swift_delete_object
*        Inputs: char *objname, CURL_HANDLE *curl_handle
*       Summary: For Swift backends, delete object "objname", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_swift_delete_object(char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;
	char delete_command[10];
	FILE *swift_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, errcode, ret;
	int32_t num_retries;

	/* For SWIFTTOKEN backend - token not set situation */
	if (swift_auth_string[0] == 0)
		return 401;

	sprintf(header_filename, "/dev/shm/swiftdeletehead%s.tmp",
		curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");
	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	strcpy(delete_command, "DELETE");

	chunk = NULL;
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");

	ASPRINTF(&url, "%s/%s/%s", swift_url_string, SWIFT_CONTAINER, objname);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, delete_command);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

	HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(swift_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(swift_header_fptr);
	if (ret_val < 0) {
		fclose(swift_header_fptr);
		unlink(header_filename);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	fclose(swift_header_fptr);
	UNLINK(header_filename);

	return ret_val;
errcode_handle:
	return -1;
}

/************************************************************************
*
* Function name: convert_currenttime
*        Inputs: uint8_t *date_string
*       Summary: For S3 backend, convert the current time to a format that
*                can be used for creating signature string.
*  Return value: None.
*
*************************************************************************/
void convert_currenttime(char *date_string)
{
	int32_t ret_val;
	char current_time[100];
	char wday[5];
	char month[5];
	char mday[5];
	char timestr[20];
	char year[6];
	struct tm tmp_gmtime;
	time_t tmptime;

	tmptime = time(NULL);
	gmtime_r(&tmptime, &tmp_gmtime);
	asctime_r(&tmp_gmtime, current_time);

	write_log(10, "current time %s\n", current_time);

	ret_val = sscanf(current_time, "%4s %4s %4s %19s %5s\n", wday, month, mday,
			 timestr, year);
	if (ret_val != 5)
		write_log(1, "Error: convert string %s\n", date_string);

	sprintf((char *)date_string, "%s, %s %s %s %s GMT", wday, mday, month,
		year, timestr);

	write_log(10, "converted string %s\n", date_string);
}

/************************************************************************
*
* Function name: compute_hmac_sha1
*        Inputs: uint8_t *input_str, uint8_t *output_str,
*                uint8_t *key, int32_t *outputlen
*       Summary: For S3 backend, compute HMAC-SHA1 string for signature use.
*  Return value: None.
*
*************************************************************************/
void compute_hmac_sha1(uint8_t *input_str, uint8_t *output_str,
		       char *key, int32_t *outputlen)
{
	uint8_t finalhash[4096];
	uint32_t len_finalhash;
	HMAC_CTX myctx;
	/* int32_t count; */

	write_log(10, "key: %s\n", key);
	write_log(10, "input: %s\n", input_str);
	write_log(10, "%d, %d\n", strlen((char *)key),
		  strlen((char *)input_str));
	HMAC_CTX_init(&myctx);

	HMAC_Init_ex(&myctx, key, strlen((char *)key), EVP_sha1(), NULL);
	HMAC_Update(&myctx, input_str, strlen((char *)input_str));
	HMAC_Final(&myctx, finalhash, &len_finalhash);
	HMAC_CTX_cleanup(&myctx);

	memcpy(output_str, finalhash, len_finalhash);
	output_str[len_finalhash] = 0;
	*outputlen = len_finalhash;

	/* for (count = 0; count < len_finalhash; count++) */
	/* write_log(10, "%02X", finalhash[count]); */
	write_log(10, "\n");
}

/************************************************************************
*
* Function name: generate_S3_sig
*        Inputs: uint8_t *method, uint8_t *date_string,
*                uint8_t *sig_string, uint8_t *resource_string,
		 HCFS_encode_object_meta
*       Summary: Compute the signature string for S3 backend.
*  Return value: None.
*
*************************************************************************/
void generate_S3_sig(char *method, char *date_string,
		     char *sig_string, char *resource_string,
		     HTTP_meta *object_meta)
{
	char sig_temp1[4096] = {0};
	uint8_t sig_temp2[64] = {0};
	int32_t len_signature, hashlen;
	char header[1024] = {0};

	if (object_meta != NULL) {
		int32_t i;

		for (i = 0; i < object_meta->count; i++) {
			char tmp[1024] = {0};

			sprintf(tmp, "x-amz-meta-%s:%s\n",
				object_meta->data[2 * i],
				object_meta->data[2 * i + 1]);
			strcat(header, tmp);
		}
	}

	convert_currenttime(date_string);
	if (object_meta != NULL) {
		sprintf(sig_temp1, "%s\n\n\n%s\n%s/%s", method, date_string,
			header, resource_string);
	} else {
		sprintf(sig_temp1, "%s\n\n\n%s\n/%s", method, date_string,
			resource_string);
	}
	write_log(10, "sig temp1: %s\n", sig_temp1);
	compute_hmac_sha1((uint8_t *)sig_temp1, sig_temp2, S3_SECRET,
			  &hashlen);
	write_log(10, "sig temp2: %s\n", sig_temp2);
	b64encode_str(sig_temp2, sig_string, &len_signature, hashlen);

	write_log(10, "final sig: %s, %d\n", sig_string, hashlen);
}

/************************************************************************
*
* Function name: hcfs_S3_list_container
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: For S3 backends, list the current bucket.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_S3_list_container(CURL_HANDLE *curl_handle)
{
	/* TODO: How to actually export the list of objects
	 * to other functions
	 */
	struct curl_slist *chunk = NULL;
	CURLcode res;
	FILE *S3_list_header_fptr, *S3_list_body_fptr;
	CURL *curl;
	char header_filename[100], body_filename[100];
	char date_string[100];
	char date_string_header[100];
	char AWS_auth_string[200];
	char S3_signature[200];
	char resource[200];
	int32_t ret_val, errcode;
	int32_t num_retries;

	sprintf(header_filename, "/dev/shm/S3listhead%s.tmp", curl_handle->id);
	sprintf(body_filename, "/dev/shm/S3listbody%s.tmp", curl_handle->id);
	sprintf(resource, "%s", S3_BUCKET);

	curl = curl_handle->curl;

	S3_list_header_fptr = fopen(header_filename, "w+");
	if (S3_list_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	S3_list_body_fptr = fopen(body_filename, "w+");
	if (S3_list_body_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		fclose(S3_list_header_fptr);
		return -1;
	}

	generate_S3_sig("GET", date_string, S3_signature, resource, NULL);

	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
		S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;

	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, "Content-Length:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, S3_BUCKET_URL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_list_header_fptr);

	curl_easy_setopt(curl, CURLOPT_WRITEDATA, S3_list_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);

	HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(S3_list_header_fptr);
		unlink(header_filename);
		fclose(S3_list_body_fptr);
		unlink(body_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	ret_val = parse_S3_list_header(S3_list_header_fptr);

	if ((ret_val >= 200) && (ret_val < 300))
		dump_S3_list_body(S3_list_body_fptr);
	/*TODO: add retry routines somewhere for failed attempts*/

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	write_log(10, "return val is: %d\n", ret_val);

	fclose(S3_list_header_fptr);
	unlink(header_filename);
	fclose(S3_list_body_fptr);
	unlink(body_filename);

	curl_slist_free_all(chunk);

	return ret_val;
}
/************************************************************************
*
* Function name: hcfs_S3_test_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: For S3 backends, displays information of the account.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_S3_test_backend(CURL_HANDLE *curl_handle)
{
	/*TODO: Finish S3 test */
	UNUSED(curl_handle);
	return -1;
}

int32_t http_is_success(int32_t code)
{
	if ((code >= 200) && (code < 300))
		return TRUE;

	return FALSE;
}
int32_t _swift_http_can_retry(int32_t code)
{
	if (hcfs_system->system_going_down == TRUE)
		return FALSE;
	switch (code) {
	case 401:
		return TRUE;
	case 408:
		return TRUE;
	case 500:
		return TRUE;
	case 503:
		return TRUE;
	case 504:
		return TRUE;
	default:
		break;
	}

	return FALSE;
}
int32_t _S3_http_can_retry(int32_t code)
{
	if (hcfs_system->system_going_down == TRUE)
		return FALSE;
	switch (code) {
	case 408:
		return TRUE;
	case 500:
		return TRUE;
	case 503:
		return TRUE;
	case 504:
		return TRUE;
	default:
		break;
	}

	return FALSE;
}

/************************************************************************
*
* Function name: hcfs_init_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Initialize backend for curl handle "curl_handle"
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	int32_t ret_val, num_retries;
	hcfs_init_backend_t *init_ftn = NULL;
	hcfs_destory_backend_t *destroy_ftn = NULL;

	ret_val = ignore_sigpipe();
	if (ret_val < 0)
		return ret_val;

	/* If init is successful, curl_backend is set to the current
	 * backend setting. Otherwise, it is set to NONE.
	 */
	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
		init_ftn = hcfs_init_swift_backend;
		destroy_ftn = hcfs_destroy_swift_backend;
		break;
	case GOOGLEDRIVE:
		init_ftn = hcfs_init_gdrive_backend;
		destroy_ftn = hcfs_destroy_gdrive_backend;
		break;
	default:
		break;
	}

	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
	case GOOGLEDRIVE:
		write_log(2, "Connecting to %s backend\n",
			  CURRENT_BACKEND == GOOGLEDRIVE ? "google drive"
							 : "swift");
		num_retries = 0;
		ret_val = init_ftn(curl_handle);
		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (curl_handle->curl != NULL)
				destroy_ftn(curl_handle->curl);
			ret_val = init_ftn(curl_handle);
		}
		if (http_is_success(ret_val) == TRUE) {
			curl_handle->curl_backend = CURRENT_BACKEND;
		} else {
			if (curl_handle->curl != NULL)
				destroy_ftn(curl_handle->curl);
			curl_handle->curl_backend = NONE;
			curl_handle->curl = NULL;
		}
		break;
	case S3:
		ret_val = hcfs_init_S3_backend(curl_handle);
		if (http_is_success(ret_val) == TRUE) {
			curl_handle->curl_backend = S3;
		} else {
			if (curl_handle->curl != NULL)
				hcfs_destroy_S3_backend(curl_handle->curl);
			curl_handle->curl_backend = NONE;
			curl_handle->curl = NULL;
		}
		break;
	default:
		ret_val = -1;
		curl_handle->curl_backend = NONE;
		break;
	}

	return ret_val;
}

/************************************************************************
*
* Function name: hcfs_destroy_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Cleanup the curl setup in curl_handle
*  Return value: None.
*
*************************************************************************/
void hcfs_destroy_backend(CURL_HANDLE *curl_handle)
{
	if (curl_handle->curl_backend == NONE)
		return;
	switch (CURRENT_BACKEND) {
	case GOOGLEDRIVE:
		hcfs_destroy_gdrive_backend(curl_handle->curl);
		break;
	case SWIFT:
	case SWIFTTOKEN:
		hcfs_destroy_swift_backend(curl_handle->curl);
		break;
	case S3:
		hcfs_destroy_S3_backend(curl_handle->curl);
		break;
	default:
		break;
	}
	curl_handle->curl_backend = NONE;
	curl_handle->curl = NULL;
}

/************************************************************************
*
* Function name: hcfs_test_backend
*        Inputs: CURL_HANDLE *curl_handle
*      Summary : Use HEAD / v1 / { account } to displays information of
*                the account. This function is called by monitor. In
*                order to detect connection errors, it WILL NOT retry
*                connection
*  Return value: Return code from request(HTTP return code), or -1 if error.
*                It should return HTTP_204_NO_CONTENT if success.
*
*************************************************************************/
int32_t hcfs_test_backend(CURL_HANDLE *curl_handle)
{
	int32_t ret_val;
	hcfs_test_backend_t *test_backend_ftn;
	hcfs_reauth_t *reauth_ftn;

	ret_val = ignore_sigpipe();
	if (ret_val < 0)
		return ret_val;

	if (curl_handle->curl_backend == NONE) {
		ret_val = hcfs_init_backend(curl_handle);
		if (http_is_success(ret_val) == FALSE) {
			write_log(5, "Error connecting to backend\n");
			sleep(5);
			return ret_val;
		}
	}
	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
		test_backend_ftn = hcfs_swift_test_backend;
		reauth_ftn = hcfs_swift_reauth;
		break;
	case GOOGLEDRIVE:
		test_backend_ftn = hcfs_gdrive_test_backend;
		reauth_ftn = hcfs_gdrive_reauth;
		break;
	case S3:
		test_backend_ftn = hcfs_S3_test_backend;
		reauth_ftn = hcfs_S3_reauth;
		break;
	default:
		ret_val = -1;
		goto out;
	}

	ret_val = test_backend_ftn(curl_handle);
	if (ret_val == 401) {
		write_log(2, "Retrying backend login");
		ret_val = reauth_ftn(curl_handle);
		if ((ret_val < 200) || (ret_val > 299))
			return ret_val;
		ret_val = test_backend_ftn(curl_handle);
	}

out:

	return ret_val;
}

/************************************************************************
*
* Function name: hcfs_S3_list_container
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: List the current container (Bucket).
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
/* TODO: nothing is actually returned in list container. FIX THIS*/
int32_t hcfs_list_container(FILE *fptr,
			    CURL_HANDLE *curl_handle,
			    added_info_t *more)
{
	int32_t ret_val, num_retries;

	UNUSED(more);
	ret_val = ignore_sigpipe();
	if (ret_val < 0)
		return ret_val;

	if (curl_handle->curl_backend == NONE) {
		ret_val = hcfs_init_backend(curl_handle);
		if (http_is_success(ret_val) == FALSE) {
			write_log(5, "Error connecting to backend\n");
			sleep(5);
			return ret_val;
		}
	}

	num_retries = 0;
	write_log(10, "Debug start listing container\n");
	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
		ret_val = hcfs_swift_list_container(curl_handle);

		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			ret_val = hcfs_swift_list_container(curl_handle);
		}

		break;
	case GOOGLEDRIVE:
		if (!more) {
			ret_val = -EINVAL;
			break;
		}
		ret_val = hcfs_gdrive_list_container(fptr, curl_handle, more);

		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_gdrive_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			ret_val =
			    hcfs_gdrive_list_container(fptr, curl_handle, more);
		}

		break;
	case S3:
		ret_val = hcfs_S3_list_container(curl_handle);
		while ((!http_is_success(ret_val)) &&
		       ((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			ret_val = hcfs_S3_list_container(curl_handle);
		}

		break;
	default:
		ret_val = -1;
		break;
	}
	return ret_val;
}

/************************************************************************
*
* Function name: hcfs_put_object
*        Inputs: FILE *fptr, char *objname, CURL_HANDLE *curl_handle
*       Summary: Put to object "objname" by reading
*                from opened file pointed by "fptr", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		    HTTP_meta *object_meta, added_info_t *more)
{
	int32_t ret_val, num_retries;
	int32_t ret, errcode;
	GOOGLEDRIVE_OBJ_INFO *gdrive_obj_info;
	int32_t (*gdrive_upload_action)(FILE *, char *, CURL_HANDLE *,
			  GOOGLEDRIVE_OBJ_INFO *);

	UNUSED(more);
	ret_val = ignore_sigpipe();
	if (ret_val < 0)
		return ret_val;

	if (curl_handle->curl_backend == NONE) {
		ret_val = hcfs_init_backend(curl_handle);
		if (http_is_success(ret_val) == FALSE) {
			write_log(5, "Error connecting to backend\n");
			sleep(5);
			return ret_val;
		}
	}

	num_retries = 0;
	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
		ret_val = hcfs_swift_put_object(fptr, objname, curl_handle,
						object_meta);
		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			FSEEK(fptr, 0, SEEK_SET);
			ret_val = hcfs_swift_put_object(
			    fptr, objname, curl_handle, object_meta);
		}
		break;
	case GOOGLEDRIVE:
		if (!more) {
			ret_val = -EINVAL;
			break;
		}
		gdrive_obj_info = (GOOGLEDRIVE_OBJ_INFO *)more;
		if (gdrive_obj_info->fileID[0] != 0) {
			/* Update object if given file ID */
			gdrive_upload_action = hcfs_gdrive_put_object;
		} else {
			/* Post new object if no ID */
			if (gdrive_obj_info->file_title[0] != 0) {
				gdrive_upload_action = hcfs_gdrive_post_object;
			} else {
				ret_val = -EINVAL;
				break;
			}
		}
		ret_val = gdrive_upload_action(fptr, objname, curl_handle,
						  gdrive_obj_info);
		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_gdrive_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			FSEEK(fptr, 0, SEEK_SET);
			ret_val = gdrive_upload_action(
			    fptr, objname, curl_handle, gdrive_obj_info);
		}
		break;
	case S3:
		ret_val =
		    hcfs_S3_put_object(fptr, objname, curl_handle, object_meta);
		while ((!http_is_success(ret_val)) &&
		       ((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			FSEEK(fptr, 0, SEEK_SET);
			ret_val = hcfs_S3_put_object(fptr, objname, curl_handle,
						     object_meta);
		}
		break;
	default:
		ret_val = -1;
		break;
	}

	return ret_val;

errcode_handle:
	return -1;
}

/************************************************************************
*
* Function name: hcfs_get_object
*        Inputs: FILE *fptr, char *objname, CURL_HANDLE *curl_handle
*       Summary: Get object "objname" and write to
*                opened file pointed by "fptr", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		    HCFS_encode_object_meta *object_meta, added_info_t *more)
{
	int32_t ret_val, num_retries;
	int32_t ret, errcode;

	UNUSED(more);
	ret_val = ignore_sigpipe();
	if (ret_val < 0)
		return ret_val;

	if (curl_handle->curl_backend == NONE) {
		ret_val = hcfs_init_backend(curl_handle);
		if (http_is_success(ret_val) == FALSE) {
			write_log(5, "Error connecting to backend\n");
			sleep(5);
			return ret_val;
		}
	}

	num_retries = 0;
	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
		ret_val = hcfs_swift_get_object(fptr, objname, curl_handle,
						object_meta);
		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			FSEEK(fptr, 0, SEEK_SET);
			FTRUNCATE(fileno(fptr), 0);
			ret_val = hcfs_swift_get_object(
			    fptr, objname, curl_handle, object_meta);
		}
		break;
	case GOOGLEDRIVE:
		if (!more) {
			ret_val = -EINVAL;
			break;
		}
		ret_val = hcfs_gdrive_get_object(fptr, objname, curl_handle,
						more);
		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_gdrive_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			FSEEK(fptr, 0, SEEK_SET);
			FTRUNCATE(fileno(fptr), 0);
			ret_val = hcfs_gdrive_get_object(
			    fptr, objname, curl_handle, more);
		}
		break;
	case S3:
		ret_val =
		    hcfs_S3_get_object(fptr, objname, curl_handle, object_meta);
		while ((!http_is_success(ret_val)) &&
		       ((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			FSEEK(fptr, 0, SEEK_SET);
			FTRUNCATE(fileno(fptr), 0);
			ret_val = hcfs_S3_get_object(fptr, objname, curl_handle,
						     object_meta);
		}
		break;
	default:
		ret_val = -1;
		break;
	}
	/* Truncate output if not successful */
	if (!http_is_success(ret_val))
		FTRUNCATE(fileno(fptr), 0);

	return ret_val;

errcode_handle:
	/* Truncate output if not successful */
	FTRUNCATE(fileno(fptr), 0);
	return -1;
}

/************************************************************************
*
* Function name: hcfs_delete_object
*        Inputs: char *objname, CURL_HANDLE *curl_handle
*       Summary: Delete object "objname", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
/* TODO: Fix handling in reauthing in SWIFT.  Now will try to reauth for
 * any HTTP error
 */
int32_t hcfs_delete_object(char *objname,
			   CURL_HANDLE *curl_handle,
			   added_info_t *more)
{
	int32_t ret_val, num_retries;

	UNUSED(more);
	ret_val = ignore_sigpipe();
	if (ret_val < 0)
		return ret_val;

	if (curl_handle->curl_backend == NONE) {
		ret_val = hcfs_init_backend(curl_handle);
		if (http_is_success(ret_val) == FALSE) {
			write_log(5, "Error connecting to backend\n");
			sleep(5);
			return ret_val;
		}
	}

	num_retries = 0;
	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
		ret_val = hcfs_swift_delete_object(objname, curl_handle);

		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			ret_val =
			    hcfs_swift_delete_object(objname, curl_handle);
		}
		break;
	case GOOGLEDRIVE:
		if (!more) {
			ret_val = -EINVAL;
			break;
		}
		ret_val = hcfs_gdrive_delete_object(objname, curl_handle, more);

		while ((!http_is_success(ret_val)) &&
		       ((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			if (ret_val == 401) {
				ret_val = hcfs_gdrive_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			ret_val = hcfs_gdrive_delete_object(objname,
							    curl_handle, more);
		}
		break;

	case S3:
		ret_val = hcfs_S3_delete_object(objname, curl_handle);
		while ((!http_is_success(ret_val)) &&
		       ((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				  "Retrying backend operation in 10 seconds");
			sleep(RETRY_INTERVAL);
			ret_val = hcfs_S3_delete_object(objname, curl_handle);
		}

		break;
	default:
		ret_val = -1;
		break;
	}
	return ret_val;
}

/************************************************************************
*
* Function name: hcfs_S3_put_object
*        Inputs: FILE *fptr, char *objname, CURL_HANDLE *curl_handle
*       Summary: For S3 backends, put to object "objname" by reading
*                from opened file pointed by "fptr", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_S3_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		       HTTP_meta *object_meta)
{
	struct curl_slist *chunk = NULL;
	off_t objsize;
	object_put_control put_control;
	CURLcode res;
	char *url = NULL;
	FILE *S3_header_fptr;
	CURL *curl;
	char header_filename[100];

	char date_string[100];
	char object_string[150];
	char date_string_header[100];
	char AWS_auth_string[200];
	char S3_signature[200];
	int32_t ret_val, ret, errcode;
	char resource[200];
	int64_t ret_pos;
	int32_t num_retries;
	struct timeval stop, start, diff;
	double time_spent;
	int64_t xfer_thpt;

	sprintf(header_filename, "/dev/shm/s3puthead%s.tmp", curl_handle->id);
	sprintf(resource, "%s/%s", S3_BUCKET, objname);
	curl = curl_handle->curl;

	S3_header_fptr = fopen(header_filename, "w+");

	if (S3_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	generate_S3_sig("PUT", date_string, S3_signature, resource,
			object_meta);

	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
		S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;

	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);
	if (object_meta != NULL) {
		int32_t i;

		for (i = 0; i < object_meta->count; i++) {
			sprintf(object_string, "x-amz-meta-%s: %s",
				object_meta->data[2 * i],
				object_meta->data[2 * i + 1]);
			chunk = curl_slist_append(chunk, object_string);
		}
	}

	FSEEK(fptr, 0, SEEK_END);
	FTELL(fptr);
	objsize = ret_pos;
	FSEEK(fptr, 0, SEEK_SET);

	if (objsize < 0) {
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

	put_control.fptr = fptr;
	put_control.object_size = objsize;
	put_control.remaining_size = objsize;

	ASPRINTF(&url, "%s/%s", S3_BUCKET_URL, objname);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_header_fptr);

	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_PUT, 1L);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, objsize);
	curl_easy_setopt(curl, CURLOPT_READDATA, (void *)&put_control);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);

	TIMEIT(HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(S3_header_fptr);
	write_log(9, "http return code: %d", ret_val);
	if (ret_val < 0) {
		fclose(S3_header_fptr);
		unlink(header_filename);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	fclose(S3_header_fptr);
	S3_header_fptr = NULL;
	UNLINK(header_filename);
	if (http_is_success(ret_val)) {
		/* Record xfer throughput */
		COMPUTE_THROUGHPUT();
		/* Update xfer statistics if successful */
		change_xfer_meta(objsize, 0, xfer_thpt, 1);
		write_log(10,
			  "Upload obj %s, size %llu, in %f seconds, %d KB/s\n",
			  objname, objsize, time_spent, xfer_thpt);
	} else {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
	}

	return ret_val;

errcode_handle:
	write_log(9, "Something wrong in curl\n");
	if (S3_header_fptr == NULL) {
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
	}

	return -1;
}

/************************************************************************
*
* Function name: hcfs_S3_get_object
*        Inputs: FILE *fptr, char *objname, CURL_HANDLE *curl_handle
*       Summary: For S3 backends, get object "objname" and write to
*                opened file pointed by "fptr", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_S3_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		       HCFS_encode_object_meta *object_meta)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;
	FILE *S3_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, ret, errcode;

	char date_string[100];
	char date_string_header[100];
	char AWS_auth_string[200];
	char S3_signature[200];
	char resource[200];
	int32_t num_retries;
	int64_t ret_pos;
	off_t objsize;
	struct timeval stop, start, diff;
	double time_spent;
	int64_t xfer_thpt;

	sprintf(header_filename, "/dev/shm/s3gethead%s.tmp", curl_handle->id);

	sprintf(resource, "%s/%s", S3_BUCKET, objname);

	curl = curl_handle->curl;

	S3_header_fptr = fopen(header_filename, "w+");
	if (S3_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	generate_S3_sig("GET", date_string, S3_signature, resource, NULL);
	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
		S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;

	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	ASPRINTF(&url, "%s/%s", S3_BUCKET_URL, objname);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_header_fptr);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_fn);

	TIMEIT(HTTP_PERFORM_RETRY(curl));
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(S3_header_fptr);
	if (ret_val < 0) {
		fclose(S3_header_fptr);
		unlink(header_filename);
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	/* get object meta data */
	if (http_is_success(ret_val) && object_meta) {
		char header[1024] = {0};

		FSEEK(S3_header_fptr, 0, SEEK_SET);
		fread(header, sizeof(char), 1000, S3_header_fptr);
		write_log(10, "download object %s header:\n%s", objname,
			  header);
		parse_http_header_coding_meta(
		    object_meta, header, "x-amz-meta-", "comp", "enc", "nonce");

		/* Record xfer throughput */
		FSEEK(fptr, 0, SEEK_END);
		FTELL(fptr);
		objsize = ret_pos;
		FSEEK(fptr, 0, SEEK_SET);
		COMPUTE_THROUGHPUT();
		/* Update xfer statistics if successful */
		change_xfer_meta(0, objsize, xfer_thpt, 1);
		write_log(
		    10, "Download obj %s, size %llu, in %f seconds, %d KB/s\n",
		    objname, objsize, time_spent, xfer_thpt);
	} else {
		/* We still need to record this failure for xfer throughput */
		change_xfer_meta(0, 0, 0, 1);
	}

	fclose(S3_header_fptr);
	S3_header_fptr = NULL;
	UNLINK(header_filename);

	return ret_val;

errcode_handle:
	if (S3_header_fptr == NULL) {
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
	}

	return -1;
}

/************************************************************************
*
* Function name: hcfs_S3_delete_object
*        Inputs: char *objname, CURL_HANDLE *curl_handle
*       Summary: For S3 backends, delete object "objname", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int32_t hcfs_S3_delete_object(char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char *url = NULL;
	char delete_command[10];

	FILE *S3_header_fptr;
	CURL *curl;
	char header_filename[100];
	int32_t ret_val, errcode, ret;
	char date_string[100];
	char date_string_header[100];
	char AWS_auth_string[200];
	char S3_signature[200];
	char resource[200];
	int32_t num_retries;

	sprintf(header_filename, "/dev/shm/s3deletehead%s.tmp",
		curl_handle->id);

	sprintf(resource, "%s/%s", S3_BUCKET, objname);

	curl = curl_handle->curl;

	S3_header_fptr = fopen(header_filename, "w+");
	if (S3_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
			  strerror(errcode));
		return -1;
	}

	strcpy(delete_command, "DELETE");

	generate_S3_sig("DELETE", date_string, S3_signature, resource, NULL);
	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
		S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	ASPRINTF(&url, "%s/%s", S3_BUCKET_URL, objname);

	HCFS_SET_DEFAULT_CURL();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_header_fptr);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, delete_command);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

	HTTP_PERFORM_RETRY(curl);
	update_backend_status((res == CURLE_OK), NULL);
	FREE(url);

	if (res != CURLE_OK) {
		write_log(4, "Curl op failed %s\n", curl_easy_strerror(res));
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(S3_header_fptr);
	if (ret_val < 0) {
		fclose(S3_header_fptr);
		unlink(header_filename);
		return -1;
	}

	if ((ret_val >= 500 && ret_val <= 505) ||
	    (ret_val >= 400 && ret_val <= 403))
		update_backend_status(FALSE, NULL);

	fclose(S3_header_fptr);
	UNLINK(header_filename);

	return ret_val;

errcode_handle:
	return -1;
}


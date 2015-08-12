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
	from normal ops*/

#include "hcfscurl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/hmac.h>
#include <openssl/engine.h>
#include <semaphore.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/types.h>

#include "b64encode.h"
#include "params.h"
#include "logger.h"
#include "macro.h"
#include "global.h"

#define MAX_RETRIES 5

extern SYSTEM_CONF_STRUCT system_config;

typedef struct {
	FILE *fptr;
	off_t object_size;
	off_t remaining_size;
} object_put_control;

/************************************************************************
*
* Function name: write_file_function
*        Inputs: void *ptr, size_t size, size_t nmemb, void *fstream
*       Summary: Same as fwrite but will return total bytes.
*  Return value: Total size for writing to fstream. Return 0 if error.
*
*************************************************************************/
size_t write_file_function(void *ptr, size_t size, size_t nmemb, void *fstream)
{
	size_t ret_size;
	int errcode;

	FWRITE(ptr, size, nmemb, fstream);

	return ret_size*size;

errcode_handle:
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
int parse_swift_auth_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	char *endptr;
	char temp_string[1024], temp_string2[1024];
	long ret_num;
	int retcodenum, ret_val;
	int ret, errcode;
	char to_stop;

	FSEEK(fptr, 0, SEEK_SET);
	ret_val = fscanf(fptr, "%19s %19s %19[^\r\n]\n",
			httpcode, retcode, retstatus);
	if (ret_val < 3)
		return -1;

	ATOL(retcode);
	retcodenum = (int) ret_num;

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

	sprintf(swift_auth_string, "%s %s", temp_string,
			temp_string2);

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
int parse_swift_list_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	char temp_string[1024], temp_string2[1024];
	int ret_val, retcodenum, total_objs;
	int ret, errcode;
	long ret_num;
	char *endptr, *tmpptr;

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
	retcodenum = (int) ret_num;

	if ((retcodenum < 200) || (retcodenum > 299))
		return retcodenum;

	while (!feof(fptr)) {
		tmpptr = fgets(temp_string, 1000, fptr);
		if (tmpptr == NULL) {
			write_log(0, "Error parsing in %s\n", __func__);
			return -1;
		}

		if (!strncmp(temp_string, "X-Container-Object-Count",
			sizeof("X-Container-Object-Count")-1)) {
			ret_val = sscanf(temp_string,
				"X-Container-Object-Count: %s\n", temp_string2);
			ATOL(temp_string2);
			total_objs = (int) ret_num;

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
int parse_S3_list_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	int ret, retcodenum;
	long ret_num;
	int errcode;
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
	retcodenum = (int) ret_num;

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
int parse_http_header_retcode(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	int ret, retcodenum;
	long ret_num;
	int errcode;
	char *endptr;

	FSEEK(fptr, 0, SEEK_SET);
	ret = fscanf(fptr, "%19s %19s %19s\n", httpcode, retcode,
			retstatus);
	if (ret < 3)
		return -1;

	ATOL(retcode);
	retcodenum = (int) ret_num;

	return retcodenum;

errcode_handle:
	return -1;
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
	int ret_val;

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
	int ret_val;

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
int hcfs_get_auth_swift(char *swift_user, char *swift_pass, 
	char *swift_url, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char auth_url[200];
	char user_string[1024];
	char pass_string[1024];
	int ret_val;
	FILE *fptr;
	CURL *curl;
	char filename[100];
	int errcode, ret;
	int num_retries;

	sprintf(filename, "/dev/shm/swiftauth%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	fptr = fopen(filename, "w+");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO Error in %s. Code %d, %s\n",
			__func__, errcode, strerror(errcode));
		return -1;
	}
	chunk = NULL;

	sprintf(auth_url, "%s://%s/auth/v1.0", SWIFT_PROTOCOL, swift_url);
	sprintf(user_string, "X-Storage-User: %s", swift_user);
	sprintf(pass_string, "X-Storage-Pass: %s", swift_pass);
	chunk = curl_slist_append(chunk, user_string);
	chunk = curl_slist_append(chunk, pass_string);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_PUT, 0L);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, fptr);
	curl_easy_setopt(curl, CURLOPT_URL, auth_url);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	HTTP_PERFORM_RETRY(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(fptr);
		unlink(filename);
		curl_slist_free_all(chunk);
			return -1;
	}

	ret_val = parse_swift_auth_header(fptr);

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
* Function name: hcfs_init_swift_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Initialize Swift backend for the curl handle "curl_handel".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int hcfs_init_swift_backend(CURL_HANDLE *curl_handle)
{
	char account_user_string[1000];
	int ret_code;

	curl_handle->curl = curl_easy_init();

	if (curl_handle->curl) {
		sprintf(account_user_string, "%s:%s", SWIFT_ACCOUNT,
						SWIFT_USER);

		ret_code = hcfs_get_auth_swift(account_user_string, SWIFT_PASS,
						SWIFT_URL, curl_handle);

		return ret_code;
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
int hcfs_init_S3_backend(CURL_HANDLE *curl_handle)
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
int hcfs_swift_reauth(CURL_HANDLE *curl_handle)
{
	char account_user_string[1000];
	int ret_code;

	if (curl_handle->curl != NULL)
		hcfs_destroy_swift_backend(curl_handle->curl);

	curl_handle->curl = curl_easy_init();

	if (curl_handle->curl) {
		sprintf(account_user_string, "%s:%s", SWIFT_ACCOUNT,
						SWIFT_USER);

		ret_code = hcfs_get_auth_swift(account_user_string, SWIFT_PASS,
						SWIFT_URL, curl_handle);

		return ret_code;
	}
	return -1;
}

/************************************************************************
*
* Function name: hcfs_swift_reauth
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Reset curl handle and init again for S3 backend.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int hcfs_S3_reauth(CURL_HANDLE *curl_handle)
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
void hcfs_destroy_swift_backend(CURL *curl)
{
	curl_easy_cleanup(curl);
}

/************************************************************************
*
* Function name: hcfs_destroy_S3_backend
*        Inputs: CURL *curl
*       Summary: Cleanup curl handle
*  Return value: None.
*
*************************************************************************/
void hcfs_destroy_S3_backend(CURL *curl)
{
	curl_easy_cleanup(curl);
}

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
		smaller than object size due to truncating*/
	FILE *fptr;
	size_t actual_to_read;
	object_put_control *put_control;
	int errcode;
	size_t ret_size;

	put_control = (object_put_control *) put_control1;

	if (put_control->remaining_size <= 0)
		return 0;

	fptr = put_control->fptr;
	if ((size*nmemb) > put_control->remaining_size)
		actual_to_read = put_control->remaining_size;
	else
		actual_to_read = size * nmemb;

	FREAD(ptr, 1, actual_to_read, fptr);
	put_control->remaining_size -= ret_size;

	return ret_size;
errcode_handle:
	return 0;
}

/************************************************************************
*
* Function name: hcfs_swift_list_container
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: For Swift backends, list the current container.
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int hcfs_swift_list_container(CURL_HANDLE *curl_handle)
{
	/*TODO: How to actually export the list of objects to other functions*/
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char container_string[200];
	FILE *swift_header_fptr, *swift_list_body_fptr;
	CURL *curl;
	char header_filename[100], body_filename[100];
	int ret_val, num_retries, errcode;

	sprintf(header_filename, "/dev/shm/swiftlisthead%s.tmp",
							curl_handle->id);
	sprintf(body_filename, "/dev/shm/swiftlistbody%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");
	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}

	swift_list_body_fptr = fopen(body_filename, "w+");
	if (swift_list_body_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		fclose(swift_header_fptr);
		return -1;
	}

	chunk = NULL;

	sprintf(container_string, "%s/%s", swift_url_string,
							SWIFT_CONTAINER);
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, "Content-Length:");

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_PUT, 0L);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, swift_list_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	HTTP_PERFORM_RETRY(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(swift_header_fptr);
		unlink(header_filename);
		fclose(swift_list_body_fptr);
		unlink(body_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	ret_val = parse_swift_list_header(swift_header_fptr);

	if ((ret_val >= 200) && (ret_val < 300))
		dump_list_body(swift_list_body_fptr);
	/*TODO: add retry routines somewhere for failed attempts*/

	fclose(swift_header_fptr);
	unlink(header_filename);
	fclose(swift_list_body_fptr);
	unlink(body_filename);

	curl_slist_free_all(chunk);

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
int hcfs_swift_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	off_t objsize;
	object_put_control put_control;
	CURLcode res;
	char container_string[200];
	FILE *swift_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val, ret, errcode;
	int num_retries;
	long ret_pos;

	sprintf(header_filename, "/dev/shm/swiftputhead%s.tmp",
							curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");

	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}
	chunk = NULL;

	sprintf(container_string, "%s/%s/%s",
				swift_url_string, SWIFT_CONTAINER, objname);
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");

	FSEEK(fptr, 0, SEEK_END);
	FTELL(fptr);
	objsize = ret_pos;
	FSEEK(fptr, 0, SEEK_SET);

	if (objsize < 0) {
		fclose(swift_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

	put_control.fptr = fptr;
	put_control.object_size = objsize;
	put_control.remaining_size = objsize;

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_PUT, 1L);
	curl_easy_setopt(curl, CURLOPT_READDATA, (void *) &put_control);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, objsize);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	HTTP_PERFORM_RETRY(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
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

	fclose(swift_header_fptr);
	swift_header_fptr = NULL;
	UNLINK(header_filename);

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
int hcfs_swift_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char container_string[200];

	FILE *swift_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val, ret, errcode;
	int num_retries;

	sprintf(header_filename, "/dev/shm/swiftgethead%s.tmp",
							curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");
	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}

	chunk = NULL;

	sprintf(container_string, "%s/%s/%s",
				swift_url_string, SWIFT_CONTAINER, objname);
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_PUT, 0L);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	HTTP_PERFORM_RETRY(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
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

	fclose(swift_header_fptr);
	swift_header_fptr = NULL;
	UNLINK(header_filename);

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
* Function name: hcfs_swift_delete_object
*        Inputs: char *objname, CURL_HANDLE *curl_handle
*       Summary: For Swift backends, delete object "objname", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int hcfs_swift_delete_object(char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char container_string[200];
	char delete_command[10];
	FILE *swift_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val, errcode, ret;
	int num_retries;

	sprintf(header_filename, "/dev/shm/swiftdeletehead%s.tmp",
							curl_handle->id);
	curl = curl_handle->curl;

	swift_header_fptr = fopen(header_filename, "w+");
	if (swift_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}

	strcpy(delete_command, "DELETE");
	chunk = NULL;

	sprintf(container_string, "%s/%s/%s",
				swift_url_string, SWIFT_CONTAINER, objname);
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_PUT, 0L);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, delete_command);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	HTTP_PERFORM_RETRY(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
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

	fclose(swift_header_fptr);
	UNLINK(header_filename);

	return ret_val;
errcode_handle:
	return -1;
}

/************************************************************************
*
* Function name: convert_currenttime
*        Inputs: unsigned char *date_string
*       Summary: For S3 backend, convert the current time to a format that
*                can be used for creating signature string.
*  Return value: None.
*
*************************************************************************/
void convert_currenttime(unsigned char *date_string)
{
	unsigned char current_time[100];
	unsigned char wday[5];
	unsigned char month[5];
	unsigned char mday[5];
	unsigned char timestr[20];
	unsigned char year[6];
	struct tm tmp_gmtime;
	time_t tmptime;

	tmptime = time(NULL);
	gmtime_r(&tmptime, &tmp_gmtime);
	asctime_r(&tmp_gmtime, current_time);

	write_log(10, "current time %s\n", current_time);

	sscanf(current_time, "%s %s %s %s %s\n", wday, month, mday,
							timestr, year);

	sprintf(date_string, "%s, %s %s %s %s GMT", wday, mday, month,
							year, timestr);

	write_log(10, "converted string %s\n", date_string);
}

/************************************************************************
*
* Function name: compute_hmac_sha1
*        Inputs: unsigned char *input_str, unsigned char *output_str,
*                unsigned char *key, int *outputlen
*       Summary: For S3 backend, compute HMAC-SHA1 string for signature use.
*  Return value: None.
*
*************************************************************************/
void compute_hmac_sha1(unsigned char *input_str, unsigned char *output_str,
					unsigned char *key, int *outputlen)
{
	unsigned char finalhash[4096];
	int len_finalhash;
	HMAC_CTX myctx;
	int count;

	write_log(10, "key: %s\n", key);
	write_log(10, "input: %s\n", input_str);
	write_log(10, "%d, %d\n", strlen(key), strlen(input_str));
	HMAC_CTX_init(&myctx);

	HMAC_Init_ex(&myctx, key, strlen(key), EVP_sha1(), NULL);
	HMAC_Update(&myctx, input_str, strlen(input_str));
	HMAC_Final(&myctx, finalhash, &len_finalhash);
	HMAC_CTX_cleanup(&myctx);

	memcpy(output_str, finalhash, len_finalhash);
	output_str[len_finalhash] = 0;
	*outputlen = len_finalhash;

	for (count = 0; count < len_finalhash; count++)
		write_log(10, "%02X", finalhash[count]);
	write_log(10, "\n");
}

/************************************************************************
*
* Function name: generate_S3_sig
*        Inputs: unsigned char *method, unsigned char *date_string,
*                unsigned char *sig_string, unsigned char *resource_string
*       Summary: Compute the signature string for S3 backend.
*  Return value: None.
*
*************************************************************************/
void generate_S3_sig(unsigned char *method, unsigned char *date_string,
		unsigned char *sig_string, unsigned char *resource_string)
{
	unsigned char sig_temp1[4096], sig_temp2[4096];
	int len_signature, hashlen;

	convert_currenttime(date_string);
	sprintf(sig_temp1, "%s\n\n\n%s\n/%s", method, date_string,
							resource_string);
	write_log(10, "sig temp1: %s\n", sig_temp1);
	compute_hmac_sha1(sig_temp1, sig_temp2, S3_SECRET, &hashlen);
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
int hcfs_S3_list_container(CURL_HANDLE *curl_handle)
{
	/*TODO: How to actually export the list of objects
		to other functions*/
	struct curl_slist *chunk = NULL;
	CURLcode res;
	FILE *S3_list_header_fptr, *S3_list_body_fptr;
	CURL *curl;
	char header_filename[100], body_filename[100];
	unsigned char date_string[100];
	char date_string_header[100];
	unsigned char AWS_auth_string[200];
	unsigned char S3_signature[200];
	unsigned char resource[200];
	int ret_val, errcode;
	int num_retries;

	sprintf(header_filename, "/dev/shm/S3listhead%s.tmp", curl_handle->id);
	sprintf(body_filename, "/dev/shm/S3listbody%s.tmp", curl_handle->id);
	sprintf(resource, "%s/", S3_BUCKET);

	curl = curl_handle->curl;

	S3_list_header_fptr = fopen(header_filename, "w+");
	if (S3_list_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}

	S3_list_body_fptr = fopen(body_filename, "w+");
	if (S3_list_body_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		fclose(S3_list_header_fptr);
		return -1;
	}

	generate_S3_sig("GET", date_string, S3_signature, resource);

	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;

	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, "Content-Length:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_PUT, 0L);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_list_header_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, S3_list_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_URL, S3_BUCKET_URL);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	HTTP_PERFORM_RETRY(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
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

	write_log(10, "return val is: %d\n", ret_val);

	fclose(S3_list_header_fptr);
	unlink(header_filename);
	fclose(S3_list_body_fptr);
	unlink(body_filename);

	curl_slist_free_all(chunk);

	return ret_val;
}

int _http_is_success(int code)
{
	if ((code >= 200) && (code < 300))
		return TRUE;

	return FALSE;
}
int _swift_http_can_retry(int code)
{
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
int _S3_http_can_retry(int code)
{
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
int hcfs_init_backend(CURL_HANDLE *curl_handle)
{
	int ret_val, num_retries;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		write_log(2, "Connecting to Swift backend\n");
		num_retries = 0;
		ret_val = hcfs_init_swift_backend(curl_handle);
		while ((!_http_is_success(ret_val)) &&
			((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			if (curl_handle->curl != NULL)
				hcfs_destroy_swift_backend(curl_handle->curl);
			ret_val = hcfs_init_swift_backend(curl_handle);
		}
		break;
	case S3:
		ret_val = hcfs_init_S3_backend(curl_handle);
		break;
	default:
		ret_val = -1;
		break;
	}

	return ret_val;
}

/************************************************************************
*
* Function name: hcfs_destroy_backend
*        Inputs: CURL *curl
*       Summary: Cleanup the curl handle "curl"
*  Return value: None.
*
*************************************************************************/
void hcfs_destroy_backend(CURL *curl)
{
	switch (CURRENT_BACKEND) {
	case SWIFT:
		hcfs_destroy_swift_backend(curl);
		break;
	case S3:
		hcfs_destroy_S3_backend(curl);
		break;
	default:
		break;
	}
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
int hcfs_list_container(CURL_HANDLE *curl_handle)
{
	int ret_val, num_retries;

	num_retries = 0;
	write_log(10, "Debug start listing container\n");
	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_swift_list_container(curl_handle);

		while ((!_http_is_success(ret_val)) &&
			((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			ret_val = hcfs_swift_list_container(curl_handle);
		}

		break;
	case S3:
		ret_val = hcfs_S3_list_container(curl_handle);
		while ((!_http_is_success(ret_val)) &&
			((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
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
int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	int ret_val, num_retries;
	int ret, errcode;

	num_retries = 0;
	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_swift_put_object(fptr, objname, curl_handle);
		while ((!_http_is_success(ret_val)) &&
			((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			FSEEK(fptr, 0, SEEK_SET);
			ret_val = hcfs_swift_put_object(fptr, objname,
				curl_handle);
		}
		break;
	case S3:
		ret_val = hcfs_S3_put_object(fptr, objname, curl_handle);
		while ((!_http_is_success(ret_val)) &&
			((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			FSEEK(fptr, 0, SEEK_SET);
			ret_val = hcfs_S3_put_object(fptr, objname,
				curl_handle);
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
/* TODO: Fix handling in reauthing in SWIFT.
	Now will try to reauth for any HTTP error*/
int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	int ret_val, num_retries;
	int ret, errcode;

	num_retries = 0;
	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_swift_get_object(fptr, objname, curl_handle);
		while ((!_http_is_success(ret_val)) &&
			((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			FSEEK(fptr, 0, SEEK_SET);
			FTRUNCATE(fileno(fptr), 0);
			ret_val = hcfs_swift_get_object(fptr, objname,
				curl_handle);
		}
		break;
	case S3:
		ret_val = hcfs_S3_get_object(fptr, objname, curl_handle);
		while ((!_http_is_success(ret_val)) &&
			((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			FSEEK(fptr, 0, SEEK_SET);
			FTRUNCATE(fileno(fptr), 0);
			ret_val = hcfs_S3_get_object(fptr, objname,
				curl_handle);
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
* Function name: hcfs_delete_object
*        Inputs: char *objname, CURL_HANDLE *curl_handle
*       Summary: Delete object "objname", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
/* TODO: Fix handling in reauthing in SWIFT.
	Now will try to reauth for any HTTP error*/
int hcfs_delete_object(char *objname, CURL_HANDLE *curl_handle)
{
	int ret_val, num_retries;

	num_retries = 0;
	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_swift_delete_object(objname, curl_handle);

		while ((!_http_is_success(ret_val)) &&
			((_swift_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			if (ret_val == 401) {
				ret_val = hcfs_swift_reauth(curl_handle);
				if ((ret_val < 200) || (ret_val > 299))
					continue;
			}
			ret_val = hcfs_swift_delete_object(objname,
					curl_handle);
		}
		break;
	case S3:
		ret_val = hcfs_S3_delete_object(objname, curl_handle);
		while ((!_http_is_success(ret_val)) &&
			((_S3_http_can_retry(ret_val)) &&
			(num_retries < MAX_RETRIES))) {
			num_retries++;
			write_log(2,
				"Retrying backend operation in 10 seconds");
			sleep(10);
			ret_val = hcfs_S3_delete_object(objname,
				curl_handle);
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
int hcfs_S3_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	off_t objsize;
	object_put_control put_control;
	CURLcode res;
	char container_string[200];
	FILE *S3_header_fptr;
	CURL *curl;
	char header_filename[100];

	unsigned char date_string[100];
	char date_string_header[100];
	unsigned char AWS_auth_string[200];
	unsigned char S3_signature[200];
	int ret_val, ret, errcode;
	unsigned char resource[200];
	long ret_pos;
	int num_retries;

	sprintf(header_filename, "/dev/shm/s3puthead%s.tmp", curl_handle->id);
	sprintf(resource, "%s/%s", S3_BUCKET, objname);
	curl = curl_handle->curl;

	S3_header_fptr = fopen(header_filename, "w+");

	if (S3_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}

	generate_S3_sig("PUT", date_string, S3_signature, resource);

	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;

	sprintf(container_string, "%s/%s", S3_BUCKET_URL, objname);
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

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


	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(curl, CURLOPT_PUT, 1L);
	curl_easy_setopt(curl, CURLOPT_READDATA, (void *) &put_control);
	curl_easy_setopt(curl, CURLOPT_INFILESIZE, objsize);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	HTTP_PERFORM_RETRY(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
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
* Function name: hcfs_S3_get_object
*        Inputs: FILE *fptr, char *objname, CURL_HANDLE *curl_handle
*       Summary: For S3 backends, get object "objname" and write to
*                opened file pointed by "fptr", using curl handle
*                pointed by "curl_handle".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*
*************************************************************************/
int hcfs_S3_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char container_string[200];

	FILE *S3_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val, ret, errcode;

	unsigned char date_string[100];
	char date_string_header[100];
	unsigned char AWS_auth_string[200];
	unsigned char S3_signature[200];
	unsigned char resource[200];
	int num_retries;

	sprintf(header_filename, "/dev/shm/s3gethead%s.tmp", curl_handle->id);

	sprintf(resource, "%s/%s", S3_BUCKET, objname);

	curl = curl_handle->curl;

	S3_header_fptr = fopen(header_filename, "w+");
	if (S3_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}

	generate_S3_sig("GET", date_string, S3_signature, resource);
	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;

	sprintf(container_string, "%s/%s", S3_BUCKET_URL, objname);
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_PUT, 0L);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	HTTP_PERFORM_RETRY(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
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
int hcfs_S3_delete_object(char *objname, CURL_HANDLE *curl_handle)
{
	struct curl_slist *chunk = NULL;
	CURLcode res;
	char container_string[200];
	char delete_command[10];

	FILE *S3_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val, errcode, ret;
	unsigned char date_string[100];
	char date_string_header[100];
	unsigned char AWS_auth_string[200];
	unsigned char S3_signature[200];
	unsigned char resource[200];
	int num_retries;

	sprintf(header_filename, "/dev/shm/s3deletehead%s.tmp",
						curl_handle->id);

	sprintf(resource, "%s/%s", S3_BUCKET, objname);

	curl = curl_handle->curl;

	S3_header_fptr = fopen(header_filename, "w+");
	if (S3_header_fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__,
			errcode, strerror(errcode));
		return -1;
	}

	strcpy(delete_command, "DELETE");

	generate_S3_sig("DELETE", date_string, S3_signature, resource);
	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	write_log(10, "%s\n", AWS_auth_string);

	chunk = NULL;

	sprintf(container_string, "%s/%s", S3_BUCKET_URL, objname);
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
	curl_easy_setopt(curl, CURLOPT_PUT, 0L);
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, delete_command);
	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	HTTP_PERFORM_RETRY(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
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

	fclose(S3_header_fptr);
	UNLINK(header_filename);

	return ret_val;

errcode_handle:
	return -1;
}

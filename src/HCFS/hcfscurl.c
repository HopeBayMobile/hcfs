/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfscurl.c
* Abstract: The c source code file for CURL operations.
*
* Revision History
* 2015/2/17 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

/* Implemented routines to parse http return code to distinguish errors
	from normal ops*/
/*TODO: Retry mechanism if HTTP operations failed due to timeout */
#include "hcfscurl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/hmac.h>
#include <openssl/engine.h>
#include <semaphore.h>
#include <curl/curl.h>

#include "b64encode.h"
#include "params.h"

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
*  Return value: Total size for writing to fstream
*
*************************************************************************/
size_t write_file_function(void *ptr, size_t size, size_t nmemb, void *fstream)
{
	size_t total_size;

	total_size = fwrite(ptr, size, nmemb, fstream);

	return total_size*size;
}

/************************************************************************
*
* Function name: parse_auth_header
*        Inputs: FILE *fptr
*       Summary: Parse the HTTP header for auth requests and return HTTP
*                return code (in integer). If successful, auth string is
*                stored in the global variable "swift_auth_string".
*  Return value: Return code from HTTP header, or -1 if error.
*
*************************************************************************/
int parse_auth_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	char temp_string[1024], temp_string2[1024];
	int ret_val, retcodenum;

	fseek(fptr, 0, SEEK_SET);
	ret_val = fscanf(fptr, "%s %s %s\n", httpcode, retcode, retstatus);
	if (ret_val < 3)
		return -1;

	retcodenum = atoi(retcode);

	if ((retcodenum < 200) || (retcodenum > 299))
		return retcodenum;

	ret_val = fscanf(fptr, "%s %s\n", temp_string, swift_url_string);

	if (ret_val < 2)
		return -1;

	ret_val = fscanf(fptr, "%s %s\n", temp_string, temp_string2);
	if (ret_val < 2)
		return -1;

	sprintf(swift_auth_string, "%s %s", temp_string, temp_string2);

	return retcodenum;
}

/************************************************************************
*
* Function name: parse_list_header
*        Inputs: FILE *fptr
*       Summary: Parse the HTTP header for Swift list requests and return HTTP
*                return code (in integer).
*  Return value: Return code from HTTP header, or -1 if error.
*
*************************************************************************/
int parse_list_header(FILE *fptr)
{
	char httpcode[20], retcode[20], retstatus[20];
	char temp_string[1024], temp_string2[1024];
	int ret_val, retcodenum, total_objs;

	fseek(fptr, 0, SEEK_SET);
	ret_val = fscanf(fptr, "%s %s", httpcode, retcode);
	if (ret_val < 2)
		return -1;

	fgets(retstatus, 19, fptr);
	retcodenum = atoi(retcode);

	if ((retcodenum < 200) || (retcodenum > 299))
		return retcodenum;

	while (!feof(fptr)) {
		fgets(temp_string, 1000, fptr);
		if (!strncmp(temp_string, "X-Container-Object-Count",
			sizeof("X-Container-Object-Count")-1)) {
			ret_val = sscanf(temp_string,
				"X-Container-Object-Count: %s\n", temp_string2);
			total_objs = atoi(temp_string2);

			printf("total objects %d\n", total_objs);

			return retcodenum;
		}
		memset(temp_string, 0, 1000);
	}
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
	char temp_string[1024], temp_string2[1024];
	int ret_val, retcodenum, total_objs;

	fseek(fptr, 0, SEEK_SET);
	ret_val = fscanf(fptr, "%s %s", httpcode, retcode);
	if (ret_val < 2)
		return -1;

	fgets(retstatus, 19, fptr);
	retcodenum = atoi(retcode);

	return retcodenum;
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
	int ret_val, retcodenum;

	fseek(fptr, 0, SEEK_SET);
	ret_val = fscanf(fptr, "%s %s %s\n", httpcode, retcode, retstatus);
	if (ret_val < 3)
		return -1;

	retcodenum = atoi(retcode);

	return retcodenum;
}

/************************************************************************
*
* Function name: dump_list_body
*        Inputs: FILE *fptr
*       Summary: For Swift list requests, dump to stdout the content of results.
*  Return value: None.
*
*************************************************************************/
void dump_list_body(FILE *fptr)
{
	char temp_string[1024];
	int ret_val;

	fseek(fptr, 0, SEEK_SET);
	while (!feof(fptr)) {
		ret_val = fscanf(fptr, "%s\n", temp_string);
		if (ret_val < 1)
			break;
		printf("%s\n", temp_string);
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
		printf("%s", temp_string);
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
int hcfs_get_auth_swift(char *swift_user, char *swift_pass, char *swift_url,
				CURL_HANDLE *curl_handle)
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

	sprintf(filename, "/run/shm/swiftauth%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	fptr = fopen(filename, "w+");
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

	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(fptr);
		unlink(filename);
		curl_slist_free_all(chunk);

		return -1;
	}

	ret_val = parse_auth_header(fptr);

	/*TODO: add retry routines somewhere for failed attempts*/

	fclose(fptr);
	unlink(filename);

	curl_slist_free_all(chunk);

	return ret_val;
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
* Function name: hcfs_init_swift_backend
*        Inputs: CURL_HANDLE *curl_handle
*       Summary: Initialize S3 backend for the curl handle "curl_handel".
*  Return value: Return code from request (HTTP return code), or -1 if error.
*          Note: We do not need to do anything special other than curl init.
*
*************************************************************************/
int hcfs_init_S3_backend(CURL_HANDLE *curl_handle)
{
	char account_user_string[1000];
	int ret_code;

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
	char account_user_string[1000];
	int ret_code;

	if (curl_handle->curl != NULL)
		hcfs_destroy_swift_backend(curl_handle->curl);

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
	size_t total_size;
	FILE *fptr;
	size_t actual_to_read;
	object_put_control *put_control;

	put_control = (object_put_control *) put_control1;

	if (put_control->remaining_size <= 0)
		return 0;

	fptr = put_control->fptr;
	if ((size*nmemb) > put_control->remaining_size)
		actual_to_read = put_control->remaining_size;
	else
		actual_to_read = size * nmemb;

	total_size = fread(ptr, 1, actual_to_read, fptr);
	put_control->remaining_size -= total_size;

	return total_size;
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
	FILE *swift_list_header_fptr, *swift_list_body_fptr;
	CURL *curl;
	char header_filename[100], body_filename[100];
	int ret_val;

	sprintf(header_filename, "/run/shm/swiftlisthead%s.tmp",
							curl_handle->id);
	sprintf(body_filename, "/run/shm/swiftlistbody%s.tmp", curl_handle->id);
	curl = curl_handle->curl;

	swift_list_header_fptr = fopen(header_filename, "w+");
	swift_list_body_fptr = fopen(body_filename, "w+");

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
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, swift_list_body_fptr);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(swift_list_header_fptr);
		unlink(header_filename);
		fclose(swift_list_body_fptr);
		unlink(body_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	ret_val = parse_list_header(swift_list_header_fptr);

	if ((ret_val >= 200) && (ret_val < 300))
		dump_list_body(swift_list_body_fptr);
	/*TODO: add retry routines somewhere for failed attempts*/

	fclose(swift_list_header_fptr);
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
	FILE *swift_list_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val;

	sprintf(header_filename, "/run/shm/swiftputhead%s.tmp",
							curl_handle->id);
	curl = curl_handle->curl;

	swift_list_header_fptr = fopen(header_filename, "w+");

	chunk = NULL;

	sprintf(container_string, "%s/%s/%s",
				swift_url_string, SWIFT_CONTAINER, objname);
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");
	fseek(fptr, 0, SEEK_END);
	objsize = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	put_control.fptr = fptr;
	put_control.object_size = objsize;
	put_control.remaining_size = objsize;

	if (objsize < 0) {
		fclose(swift_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

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
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(swift_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(swift_list_header_fptr);
	fclose(swift_list_header_fptr);
	unlink(header_filename);

	return ret_val;
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
	off_t objsize;
	CURLcode res;
	char container_string[200];

	FILE *swift_list_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val;

	sprintf(header_filename, "/run/shm/swiftgethead%s.tmp",
							curl_handle->id);
	curl = curl_handle->curl;

	swift_list_header_fptr = fopen(header_filename, "w+");

	chunk = NULL;

	sprintf(container_string, "%s/%s/%s",
				swift_url_string, SWIFT_CONTAINER, objname);
	chunk = curl_slist_append(chunk, swift_auth_string);
	chunk = curl_slist_append(chunk, "Expect:");
	fseek(fptr, 0, SEEK_END);
	objsize = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	if (objsize < 0) {
		fclose(swift_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

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
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(swift_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(swift_list_header_fptr);
	fclose(swift_list_header_fptr);
	unlink(header_filename);

	return ret_val;
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
	off_t objsize;
	CURLcode res;
	char container_string[200];
	char delete_command[10];

	FILE *swift_list_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val;

	sprintf(header_filename, "/run/shm/swiftdeletehead%s.tmp",
							curl_handle->id);
	curl = curl_handle->curl;

	swift_list_header_fptr = fopen(header_filename, "w+");
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
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(swift_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(swift_list_header_fptr);
	fclose(swift_list_header_fptr);
	unlink(header_filename);

	return ret_val;
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

	printf("current time %s\n", current_time);

	sscanf(current_time, "%s %s %s %s %s\n", wday, month, mday,
							timestr, year);

	sprintf(date_string, "%s, %s %s %s %s GMT", wday, mday, month,
							year, timestr);

	printf("converted string %s\n", date_string);
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

	printf("key: %s\n", key);
	printf("input: %s\n", input_str);
	printf("%d, %d\n", strlen(key), strlen(input_str));
	HMAC_CTX_init(&myctx);

	HMAC_Init_ex(&myctx, key, strlen(key), EVP_sha1(), NULL);
	HMAC_Update(&myctx, input_str, strlen(input_str));
	HMAC_Final(&myctx, finalhash, &len_finalhash);
	HMAC_CTX_cleanup(&myctx);

	memcpy(output_str, finalhash, len_finalhash);
	output_str[len_finalhash] = 0;
	*outputlen = len_finalhash;

	for (count = 0; count < len_finalhash; count++)
		printf("%02X", finalhash[count]);
	printf("\n");
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
	printf("sig temp1: %s\n", sig_temp1);
	compute_hmac_sha1(sig_temp1, sig_temp2, S3_SECRET, &hashlen);
	printf("sig temp2: %s\n", sig_temp2);
	b64encode_str(sig_temp2, sig_string, &len_signature, hashlen);

	printf("final sig: %s, %d\n", sig_string, hashlen);
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
	int ret_val;

	sprintf(header_filename, "/run/shm/S3listhead%s.tmp", curl_handle->id);
	sprintf(body_filename, "/run/shm/S3listbody%s.tmp", curl_handle->id);
	sprintf(resource, "%s/", S3_BUCKET);

	curl = curl_handle->curl;

	S3_list_header_fptr = fopen(header_filename, "w+");
	S3_list_body_fptr = fopen(body_filename, "w+");

	generate_S3_sig("GET", date_string, S3_signature, resource);

	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	printf("%s\n", AWS_auth_string);

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
	res = curl_easy_perform(curl);

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

	printf("return val is: %d\n", ret_val);

	fclose(S3_list_header_fptr);
	unlink(header_filename);
	fclose(S3_list_body_fptr);
	unlink(body_filename);

	curl_slist_free_all(chunk);

	return ret_val;
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
	int ret_val;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_init_swift_backend(curl_handle);
		while ((ret_val < 200) || (ret_val > 299)) {
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
/* TODO: Fix handling in reauthing in SWIFT. Now will try
	to reauth for any HTTP error*/
/* TODO: nothing is actually returned in list container. FIX THIS*/
int hcfs_list_container(CURL_HANDLE *curl_handle)
{
	int ret_val;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_swift_list_container(curl_handle);
		while ((ret_val < 200) || (ret_val > 299)) {
			ret_val = hcfs_swift_reauth(curl_handle);
			if ((ret_val < 200) || (ret_val > 299))
				continue;
			ret_val = hcfs_swift_list_container(curl_handle);
		}
		break;
	case S3:
		ret_val = hcfs_S3_list_container(curl_handle);
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
/* TODO: Fix handling in reauthing in SWIFT. Now will try to reauth for
		any HTTP error*/
int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
{
	int ret_val;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_swift_put_object(fptr, objname, curl_handle);
		while ((ret_val < 200) || (ret_val > 299)) {
			ret_val = hcfs_swift_reauth(curl_handle);
			if ((ret_val < 200) && (ret_val > 299))
				continue;
			fseek(fptr, 0, SEEK_SET);
			ret_val = hcfs_swift_put_object(fptr, objname,
								curl_handle);
		}
		break;
	case S3:
		ret_val = hcfs_S3_put_object(fptr, objname, curl_handle);
		break;
	default:
		ret_val = -1;
		break;
	}

	return ret_val;
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
	int status;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		status = hcfs_swift_get_object(fptr, objname, curl_handle);

		while ((status < 200) || (status > 299))
			status = hcfs_swift_reauth(curl_handle);
		break;
	case S3:
		status = hcfs_S3_get_object(fptr, objname, curl_handle);
		break;
	default:
		status = -1;
		break;
	}
	return status;
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
	int ret_val;

	switch (CURRENT_BACKEND) {
	case SWIFT:
		ret_val = hcfs_swift_delete_object(objname, curl_handle);
		while (((ret_val < 200) || (ret_val > 299)) &&
					(ret_val != 404)) {
			ret_val = hcfs_swift_reauth(curl_handle);
			if ((ret_val >= 200) && (ret_val <= 299))
				ret_val = hcfs_swift_delete_object(objname,
								curl_handle);
		}
		break;
	case S3:
		ret_val = hcfs_S3_delete_object(objname, curl_handle);
		while (((ret_val < 200) || (ret_val > 299)) &&
							(ret_val != 404)) {
			ret_val = hcfs_S3_reauth(curl_handle);
			if ((ret_val >= 200) && (ret_val <= 299))
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
	int ret_val;
	unsigned char resource[200];


	sprintf(header_filename, "/run/shm/s3puthead%s.tmp", curl_handle->id);
	sprintf(resource, "%s/%s", S3_BUCKET, objname);
	curl = curl_handle->curl;

	S3_header_fptr = fopen(header_filename, "w+");

	generate_S3_sig("PUT", date_string, S3_signature, resource);

	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	printf("%s\n", AWS_auth_string);

	chunk = NULL;

	sprintf(container_string, "%s/%s", S3_BUCKET_URL, objname);
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	fseek(fptr, 0, SEEK_END);
	objsize = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	put_control.fptr = fptr;
	put_control.object_size = objsize;
	put_control.remaining_size = objsize;

	if (objsize < 0) {
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

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
	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(S3_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(S3_header_fptr);
	fclose(S3_header_fptr);
	unlink(header_filename);

	return ret_val;
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
	off_t objsize;
	CURLcode res;
	char container_string[200];

	FILE *S3_list_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val;

	unsigned char date_string[100];
	char date_string_header[100];
	unsigned char AWS_auth_string[200];
	unsigned char S3_signature[200];
	unsigned char resource[200];

	sprintf(header_filename, "/run/shm/s3gethead%s.tmp", curl_handle->id);

	sprintf(resource, "%s/%s", S3_BUCKET, objname);

	curl = curl_handle->curl;

	S3_list_header_fptr = fopen(header_filename, "w+");

	generate_S3_sig("GET", date_string, S3_signature, resource);
	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	printf("%s\n", AWS_auth_string);

	chunk = NULL;

	sprintf(container_string, "%s/%s", S3_BUCKET_URL, objname);
	chunk = curl_slist_append(chunk, "Expect:");
	chunk = curl_slist_append(chunk, date_string_header);
	chunk = curl_slist_append(chunk, AWS_auth_string);

	fseek(fptr, 0, SEEK_END);
	objsize = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	if (objsize < 0) {
		fclose(S3_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);

		return -1;
	}

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
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_list_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	res = curl_easy_perform(curl);

	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(S3_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(S3_list_header_fptr);
	fclose(S3_list_header_fptr);
	unlink(header_filename);

	return ret_val;
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
	off_t objsize;
	CURLcode res;
	char container_string[200];
	char delete_command[10];

	FILE *S3_list_header_fptr;
	CURL *curl;
	char header_filename[100];
	int ret_val;

	unsigned char date_string[100];
	char date_string_header[100];
	unsigned char AWS_auth_string[200];
	unsigned char S3_signature[200];
	unsigned char resource[200];

	sprintf(header_filename, "/run/shm/s3deletehead%s.tmp",
						curl_handle->id);

	sprintf(resource, "%s/%s", S3_BUCKET, objname);

	curl = curl_handle->curl;

	S3_list_header_fptr = fopen(header_filename, "w+");
	strcpy(delete_command, "DELETE");

	generate_S3_sig("DELETE", date_string, S3_signature, resource);
	sprintf(date_string_header, "date: %s", date_string);
	sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS,
								S3_signature);

	printf("%s\n", AWS_auth_string);

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
	curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_list_header_fptr);

	curl_easy_setopt(curl, CURLOPT_URL, container_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
		fclose(S3_list_header_fptr);
		unlink(header_filename);
		curl_slist_free_all(chunk);
		return -1;
	}

	curl_slist_free_all(chunk);
	ret_val = parse_http_header_retcode(S3_list_header_fptr);
	fclose(S3_list_header_fptr);
	unlink(header_filename);

	return ret_val;
}

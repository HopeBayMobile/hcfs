/*************************************************************************
*
* Copyright © 2014-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfscurl.h
* Abstract: The c header file for CURL operations.
*
* Revision History
* 2015/2/17 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

#ifndef GW20_HCFS_HCFSCURL_H_
#define GW20_HCFS_HCFSCURL_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <pthread.h>
#include "objmeta.h"
#include "params.h"

#define MAX_DOWNLOAD_CURL_HANDLE 4
/* For swift token control */
#define MAX_WAIT_TIME 60

typedef void added_info_t; /* Additional data curl ops needed. */

typedef struct {
	FILE *fptr;
	off_t object_size;
	off_t remaining_size;
} object_put_control;

typedef struct {
	CURL *curl;

	/*A short name representing the unique identity of the handle*/
	char id[256];
	int32_t curl_backend;
} CURL_HANDLE;

typedef struct {
	/* access storage url and token control*/
	pthread_mutex_t access_lock;
	/* control to make threads waiting for new token */
	pthread_mutex_t waiting_lock;
	pthread_cond_t waiting_cond;
} BACKEND_TOKEN_CONTROL;

extern BACKEND_TOKEN_CONTROL swifttoken_control;
extern char swift_auth_string[1024];
extern char swift_url_string[1024];

CURL_HANDLE download_curl_handles[MAX_DOWNLOAD_CURL_HANDLE];
CURL_HANDLE download_usermeta_curl_handle;
int16_t curl_handle_mask[MAX_DOWNLOAD_CURL_HANDLE];
sem_t download_curl_control_sem;
sem_t download_curl_sem;
sem_t nonread_download_curl_sem;

typedef int32_t hcfs_init_backend_t(CURL_HANDLE *);
typedef void hcfs_destory_backend_t(CURL *);
typedef int32_t hcfs_test_backend_t(CURL_HANDLE *);
typedef int32_t hcfs_reauth_t(CURL_HANDLE *);

/* Swift collections */
int32_t hcfs_get_auth_swift(char *swift_user, char *swift_pass, char *swift_url,
			CURL_HANDLE *curl_handle);
int32_t hcfs_init_swift_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_swift_backend(CURL *curl);
int32_t hcfs_swift_test_backend(CURL_HANDLE *curl_handle);
int32_t hcfs_swift_list_container(CURL_HANDLE *curl_handle);
int32_t hcfs_swift_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
			  HCFS_encode_object_meta *);
int32_t hcfs_swift_reauth(CURL_HANDLE *curl_handle);
int32_t hcfs_swift_delete_object(char *objname, CURL_HANDLE *curl_handle);
int32_t hcfs_swift_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
			  HTTP_meta *object_meta);

/* S3 collections */
int32_t hcfs_init_S3_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_S3_backend(CURL *curl);
int32_t hcfs_S3_test_backend(CURL_HANDLE *curl_handle);
int32_t hcfs_S3_list_container(CURL_HANDLE *curl_handle);
int32_t hcfs_S3_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		       HCFS_encode_object_meta *);
int32_t hcfs_S3_delete_object(char *objname, CURL_HANDLE *curl_handle);
int32_t hcfs_S3_reauth(CURL_HANDLE *curl_handle);
int32_t hcfs_S3_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		       HTTP_meta *);

/* Generic */
int32_t hcfs_get_auth_token(void);
int32_t hcfs_init_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_backend(CURL_HANDLE *curl_handle);
int32_t hcfs_test_backend(CURL_HANDLE *curl_handle);
int32_t hcfs_list_container(FILE *fptr,
			    CURL_HANDLE *curl_handle,
			    added_info_t *);
int32_t hcfs_put_object(FILE *fptr,
			char *objname,
			CURL_HANDLE *curl_handle,
			HTTP_meta *,
			added_info_t *);
int32_t hcfs_get_object(FILE *fptr,
			char *objname,
			CURL_HANDLE *curl_handle,
			HCFS_encode_object_meta *,
			added_info_t *);
int32_t hcfs_delete_object(char *objname,
			   CURL_HANDLE *curl_handle,
			   added_info_t *);
/* Tools */
#define MAX_RETRIES 5
#ifdef UNITTEST
#define RETRY_INTERVAL 0
#else
#define RETRY_INTERVAL 10
#endif

#define HCFS_SET_DEFAULT_CURL()                                                \
	do {                                                                   \
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);           \
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_fn); \
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);                   \
		curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);                    \
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);                  \
		curl_easy_setopt(curl, CURLOPT_PUT, 0L);                       \
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);            \
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);            \
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);                  \
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);                    \
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);                   \
	} while (0)

int32_t http_is_success(int32_t code);
size_t write_file_fn(void *ptr, size_t size, size_t nmemb, void *fstream);
int32_t parse_http_header_retcode(FILE *fptr);
int32_t parse_http_header_coding_meta(HCFS_encode_object_meta *object_meta,
				  char *httpheader, const char *, const char *,
				  const char *, const char *);
size_t read_file_function(void *ptr, size_t size, size_t nmemb,
			  void *put_control1);
/*
 * Marco to compute data transfer throughput.  If object size < 32KB, for
 * this computation the size is rounded up to 32KB.
 */
#define COMPUTE_THROUGHPUT()                                                   \
	do {                                                                   \
		off_t objsize_kb = (objsize <= 32768) ? 32 : objsize / 1024;   \
		time_spent = (time_spent <= 0) ? 0.001 : time_spent;           \
		xfer_thpt = (int64_t)(objsize_kb / time_spent);                \
	} while (0)

#endif /* GW20_HCFS_HCFSCURL_H_ */

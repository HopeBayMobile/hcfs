/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <semaphore.h>
#include "objmeta.h"
#include "params.h"

#define MAX_DOWNLOAD_CURL_HANDLE 16

extern SYSTEM_CONF_STRUCT system_config;

typedef struct {
	FILE *fptr;
	off_t object_size;
	off_t remaining_size;
} object_put_control;

typedef struct {
	CURL *curl;

	/*A short name representing the unique identity of the handle*/
	char id[256];
	int curl_backend;
} CURL_HANDLE;

char swift_auth_string[1024];
char swift_url_string[1024];

CURL_HANDLE download_curl_handles[MAX_DOWNLOAD_CURL_HANDLE];
short curl_handle_mask[MAX_DOWNLOAD_CURL_HANDLE];
sem_t download_curl_control_sem;
sem_t download_curl_sem;

/* Swift collections */

int hcfs_get_auth_swift(char *swift_user, char *swift_pass, char *swift_url,
			CURL_HANDLE *curl_handle);
int hcfs_init_swift_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_swift_backend(CURL *curl);
int hcfs_swift_test_backend(CURL_HANDLE *curl_handle);
int hcfs_swift_list_container(CURL_HANDLE *curl_handle);
int hcfs_swift_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
			  HCFS_encode_object_meta *);
int hcfs_swift_reauth(CURL_HANDLE *curl_handle);
int hcfs_swift_delete_object(char *objname, CURL_HANDLE *curl_handle);
int hcfs_swift_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
			  HTTP_meta *object_meta);

/* S3 collections */
int hcfs_init_S3_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_S3_backend(CURL *curl);
int hcfs_S3_test_backend(CURL_HANDLE *curl_handle);
int hcfs_S3_list_container(CURL_HANDLE *curl_handle);
int hcfs_S3_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		       HCFS_encode_object_meta *);
int hcfs_S3_delete_object(char *objname, CURL_HANDLE *curl_handle);
int hcfs_S3_reauth(CURL_HANDLE *curl_handle);
int hcfs_S3_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		       HTTP_meta *);

/* Generic */
int hcfs_init_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_backend(CURL_HANDLE *curl_handle);
int hcfs_test_backend(CURL_HANDLE *curl_handle);
int hcfs_list_container(CURL_HANDLE *curl_handle);
int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		    HTTP_meta *);
int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle,
		    HCFS_encode_object_meta *);
int hcfs_delete_object(char *objname, CURL_HANDLE *curl_handle);

int parse_http_header_coding_meta(HCFS_encode_object_meta *object_meta,
				  char *httpheader, const char *, const char *,
				  const char *, const char *);
#endif /* GW20_HCFS_HCFSCURL_H_ */

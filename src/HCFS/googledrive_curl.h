/*************************************************************************
*
* Copyright © 2016-2017 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfscurl.h
* Abstract: The c header file for CURL operations.
*
* Revision History
* 2016/12/29 Kewei created this header file.
*
**************************************************************************/

#ifndef GW20_HCFS_GOOGLEDRIVE_CURL_H_
#define GW20_HCFS_GOOGLEDRIVE_CURL_H_

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
#include "hcfscurl.h"
#include "meta.h"

#define BOUNDARY_STRING "hcfs_boundary"

typedef struct {
	sem_t op_lock;
	char hcfs_folder_id[GDRIVE_ID_LENGTH];
} GOOGLE_DRIVE_FOLDER_ID_CACHE_T;

/* Record google drive folder id */
GOOGLE_DRIVE_FOLDER_ID_CACHE_T *gdrive_folder_id_cache;

/* Upload: If file ID is empty, then perform "create" operation. Otherwiese
 * perform "update" op. Besides, if parent ID is empty, then just upload under
 * root folder.
 * Download: Do not need parent ID, but file ID is necessary.
 * Delete: Do not need parent ID, but file ID is necessary.
 * List: All fields are not necessary.
 */
typedef struct {
	FILE *fptr;
	size_t object_size;
	size_t object_remaining_size;
	char *head_string;
	int32_t head_remaining;
	char *tail_string;
	int32_t tail_remaining;
	int32_t total_remaining;
} object_post_control;

BACKEND_TOKEN_CONTROL *googledrive_token_control;
char googledrive_token[1024];

int32_t hcfs_init_gdrive_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_gdrive_backend(CURL *curl);
int32_t init_gdrive_token_control(void);
int32_t hcfs_gdrive_reauth(CURL_HANDLE *curl_handle);
int32_t hcfs_gdrive_test_backend(CURL_HANDLE *curl_handle);
int32_t hcfs_gdrive_list_container(FILE *fptr, CURL_HANDLE *curl_handle,
				   GOOGLEDRIVE_OBJ_INFO *more);
int32_t hcfs_gdrive_get_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info);
int32_t hcfs_gdrive_delete_object(char *objname,
				  CURL_HANDLE *curl_handle,
				  GOOGLEDRIVE_OBJ_INFO *obj_info);
int32_t hcfs_gdrive_put_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info);
int32_t hcfs_gdrive_post_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info);

int32_t get_gdrive_fileID(const char *file_name,
			  char *parentid,
			  char *fileid);

int32_t get_parent_id(char *id, const char *objname);

#endif

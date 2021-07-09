/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

#define BOUNDARY_STRING "rhwuirnjkscd_hcfs_boundary_testing"

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
	const char *objname;
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
int32_t query_object_id(GOOGLEDRIVE_OBJ_INFO *obj_info);

void gdrive_exp_backoff_sleep(int32_t busy_retry_times);

#endif

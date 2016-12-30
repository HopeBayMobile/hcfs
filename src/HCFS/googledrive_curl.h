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

/* Upload: If file ID is empty, then perform "create" operation. Otherwiese
 * perform "update" op. Besides, if parent ID is empty, then just upload under
 * root folder.
 * Download: Do not need parent ID, but file ID is necessary.
 * Delete: Do not need parent ID, but file ID is necessary.
 * List: All fields are not necessary.
 */
typedef struct {
	char file_title[512]; /*File name on google drive*/
	char fileID[256];     /*File ID*/
	char parentID[256];   /*Parent ID, can be empty string*/
} GOOGLEDRIVE_OBJ_INFO;

BACKEND_TOKEN_CONTROL *googledrive_token_control;
char googledrive_token[1024];

int32_t init_gdrive_token_control(void);
int32_t hcfs_gdrive_reauth(CURL_HANDLE *curl_handle);
int32_t hcfs_gdrive_list_container(FILE *fptr, CURL_HANDLE *curl_handle,
				   GOOGLEDRIVE_OBJ_INFO *more);
int32_t hcfs_init_gdrive_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_gdrive_backend(CURL *curl);

int32_t get_gdrive_fileID(const char *file_name,
			  char *parentid,
			  char *fileid);

#endif

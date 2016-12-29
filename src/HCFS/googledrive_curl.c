/*************************************************************************
*
* Copyright © 2016-2017 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfscurl.h
* Abstract: The c header file for CURL operations.
*
* Revision History
* 2015/2/17 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

#include "googledrive_curl.h"

#include "hcfscurl.h"


int32_t init_gdrive_token_control(void)
{
	if (googledrive_token_control)
		return -EEXIST;

	googledrive_token_control =
	    (BACKEND_TOKEN_CONTROL *)calloc(sizeof(BACKEND_TOKEN_CONTROL), 1);
	if (!googledrive_token_control)
		return -errno;

	pthread_mutex_init(&googledrive_token_control->access_lock, NULL);
	pthread_mutex_init(&googledrive_token_control->waiting_lock, NULL);
	pthread_cond_init(&googledrive_token_control->waiting_cond, NULL);

	return 0;
}

int32_t hcfs_init_gdrive_backend(CURL_HANDLE *curl_handle)
{
	int32_t ret_code;

	curl_handle->curl = curl_easy_init();

	if (curl_handle->curl) {
		ret_code = hcfs_get_auth_token();
		if (ret_code == 200 || swift_auth_string[0] != 0)
			return 200;
		else
			return ret_code;
	}
	return -1;
}

void hcfs_destroy_gdrive_backend(CURL *curl) { curl_easy_cleanup(curl); }

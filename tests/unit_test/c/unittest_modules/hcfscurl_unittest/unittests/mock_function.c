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
#include "hcfscurl.h"
#include "meta.h"
#include "mock_function.h"
#include <openssl/hmac.h>

#undef curl_easy_setopt
#define MOCK() printf("[MOCK] hcfscurl mock_function.c line %4d func %s\n",  __LINE__, __func__)
CURLcode curl_easy_setopt(CURL *handle, CURLoption option, ...)
{
	MOCK();
	if ((write_auth_header_flag == FALSE) &&
		(write_list_header_flag == FALSE))
		return CURLE_OK;

	if (option != CURLOPT_WRITEHEADER)
		return CURLE_OK;

	va_list alist;
	FILE *fptr;
	char buf[500];
	int32_t retcode;

	/* "let_retry" is used to test retry connection */
	if (let_retry == TRUE) {
		retcode = 503; /* 503 will make caller retry */
		let_retry = FALSE;
	} else {
		retcode = 200;
	}

	memset(buf, 0, 500);
	if (write_auth_header_flag == TRUE)
		sprintf(buf, "HTTP/1.1 %d OK\n"
		"X-Storage-Url: http://127.0.0.1/fake\n"
		"X-Auth-Token: hello_swift_auth_string\n", retcode);
	if (write_list_header_flag == TRUE)
		sprintf(buf, "http/1.1 %d OK\n"
		"X-Container-Object-Count: 5566\n", retcode);

	va_start(alist, option);
	fptr = va_arg(alist, FILE *);
	fwrite(buf, 1, strlen(buf), fptr);
	va_end(alist);
}

#undef curl_easy_getinfo
CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...){
	return CURLE_OK;
}

struct curl_slist *curl_slist_append(struct curl_slist * list,
	const char * string )
{
	MOCK();
}

void curl_slist_free_all(struct curl_slist * list)
{
	MOCK();
}

int32_t write_log(int32_t level, const char *format, ...)
{
	va_list args;
	va_start (args, format);
	//vprintf (format, args);
	fflush(stdout);
	va_end (args);

	return 0;
}

CURLcode curl_easy_perform(CURL *easy_handle)
{
	MOCK();
	if (http_perform_retry_fail == TRUE)
		return -2;

	return CURLE_OK;
}

const char *curl_easy_strerror(CURLcode errornum)
{
	MOCK();
	return "test";
}

CURL *curl_easy_init()
{
	MOCK();
	return 1;
}

void curl_easy_cleanup(CURL *handle)
{
	MOCK();
	switch (CURRENT_BACKEND) {
	case SWIFT:
	case SWIFTTOKEN:
		swift_destroy = FALSE;
		break;
	case S3:
		s3_destroy = FALSE;
		break;
	}
}

void HMAC_CTX_init(HMAC_CTX *ctx)
{
	MOCK();
}

int32_t HMAC_Init_ex(HMAC_CTX *ctx, const void *key, int32_t key_len,
	const EVP_MD *md, ENGINE *impl)
{
	MOCK();
	return 0;
}

int32_t HMAC_Update(HMAC_CTX *ctx, const uint8_t *data, size_t len)
{
	MOCK();
	return 0;
}

int32_t HMAC_Final(HMAC_CTX *ctx, uint8_t *md, uint32_t *len)
{
	MOCK();
	*len = strlen("test_hmac_final");
	strcpy(md, "test_hmac_final");
	return 0;
}

void HMAC_CTX_cleanup(HMAC_CTX *ctx)
{
	MOCK();
}

const EVP_MD *EVP_sha1(void)
{
	MOCK();
}

int32_t b64encode_str(uint8_t *inputstr,
		      char *outputstr,
		      int32_t *outlen,
		      int32_t inputlen)
{
	MOCK();
	strcpy(outputstr, "test_b64encode");
}

void update_backend_status(int32_t status, struct timespec *status_time) {}
int32_t ignore_sigpipe(void)
{
	MOCK();
	return 0;
}

int32_t change_xfer_meta(int64_t xfer_size_upload, int64_t xfer_size_download,
		     int64_t xfer_throughtput, int64_t xfer_total_obj)
{
	MOCK();
	return 0;
}
int32_t hcfs_init_gdrive_backend(CURL_HANDLE *curl_handle)
{
	return 0;
}
void hcfs_destroy_gdrive_backend(CURL *curl)
{
	return;
}

int32_t init_gdrive_token_control(void)
{
	return 0;
}
int32_t hcfs_gdrive_reauth(CURL_HANDLE *curl_handle)
{
	return 0;
}
int32_t hcfs_gdrive_test_backend(CURL_HANDLE *curl_handle)
{
	return 0;
}
int32_t hcfs_gdrive_list_container(FILE *fptr, CURL_HANDLE *curl_handle,
				   GOOGLEDRIVE_OBJ_INFO *more)
{
	return 0;
}
int32_t hcfs_gdrive_get_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	return 0;
}
int32_t hcfs_gdrive_delete_object(char *objname,
				  CURL_HANDLE *curl_handle,
				  GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	return 0;
}
int32_t hcfs_gdrive_put_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	return 0;
}
int32_t hcfs_gdrive_post_object(FILE *fptr,
			       char *objname,
			       CURL_HANDLE *curl_handle,
			       GOOGLEDRIVE_OBJ_INFO *obj_info)
{
	return 0;
}

int32_t get_gdrive_fileID(const char *file_name,
			  char *parentid,
			  char *fileid)
{
	return 0;
}

int32_t get_parent_id(char *id, const char *objname)
{
	return 0;
}

void gdrive_exp_backoff_sleep(int32_t busy_retry_times)
{
	return;
}


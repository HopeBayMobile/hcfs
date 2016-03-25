/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: mock_function.c
* Abstract: 
*
* Revision History
* 2016/1/14 Jethro add copyright.
*
**************************************************************************/
#include "hcfscurl.h"
#include "mock_params.h"
#include <openssl/hmac.h>

#undef curl_easy_setopt
CURLcode curl_easy_setopt(CURL *handle, CURLoption option, ...)
{
	if ((write_auth_header_flag == FALSE) &&
		(write_list_header_flag == FALSE))
		return CURLE_OK;

	if (option != CURLOPT_WRITEHEADER)
		return CURLE_OK;

	va_list alist;
	FILE *fptr;
	char buf[500];
	int retcode;

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
}

void curl_slist_free_all(struct curl_slist * list)
{
}

int write_log(int level, char *format, ...)
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
	if (http_perform_retry_fail == TRUE)
		return -2;

	return CURLE_OK;
}

const char *curl_easy_strerror(CURLcode errornum)
{
	return "test";
}

CURL *curl_easy_init()
{
	return 1;
}

void curl_easy_cleanup(CURL *handle)
{
	switch (CURRENT_BACKEND) {
	case SWIFT:
		swift_destroy = FALSE;
		break;
	case S3:
		s3_destroy = FALSE;
		break;
	}
}

void HMAC_CTX_init(HMAC_CTX *ctx)
{
}

int HMAC_Init_ex(HMAC_CTX *ctx, const void *key, int key_len,
	const EVP_MD *md, ENGINE *impl)
{
	return 0;
}

int HMAC_Update(HMAC_CTX *ctx, const unsigned char *data, size_t len)
{
	return 0;
}

int HMAC_Final(HMAC_CTX *ctx, unsigned char *md, unsigned int *len)
{
	*len = strlen("test_hmac_final");
	strcpy(md, "test_hmac_final");
	return 0;
}

void HMAC_CTX_cleanup(HMAC_CTX *ctx)
{
}

const EVP_MD *EVP_sha1(void)
{
}

int b64encode_str(unsigned char *inputstr, unsigned char *outputstr,
	int *outlen, int inputlen)
{
	strcpy(outputstr, "test_b64encode");
}

void update_backend_status(int status, struct timespec *status_time) {}
int ignore_sigpipe(void)
{
	return 0;
}

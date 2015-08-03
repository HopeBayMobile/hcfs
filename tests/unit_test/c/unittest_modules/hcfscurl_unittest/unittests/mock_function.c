#include "hcfscurl.h"
#include "mock_params.h"
#include <openssl/hmac.h>
//#include "global.h"

//#ifdef __CURL_CURL_H
//#undef __CURL_TYPECHECK_GCC_H

//#define curl_easy_setopt my_curl
//URLcode curl_easy_setopt(CURL *handle, CURLoption option, int parameter)
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

	if (write_auth_header_flag == TRUE)
		strcpy(buf, "HTTP/1.1 200 OK\n"
		"X-Storage-Url: http://127.0.0.1/fake\n"
		"X-Auth-Token: hello_swift_auth_string\n");
	if (write_list_header_flag == TRUE)
		strcpy(buf, "http/1.1 200 OK\n"
		"X-Container-Object-Count: 5566\n");

	va_start(alist, option);
	fptr = va_arg(alist, FILE *);
	fwrite(buf, 1, strlen(buf), fptr);
	va_end(alist);

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

void curl_easy_cleanup(CURL * handle)
{
}

void HMAC_CTX_init(HMAC_CTX *ctx)
{
}

int HMAC_Init_ex(HMAC_CTX *ctx, const void *key, int key_len,
	const EVP_MD *md, ENGINE *impl)
{
}

int HMAC_Update(HMAC_CTX *ctx, const unsigned char *data, size_t len)
{
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

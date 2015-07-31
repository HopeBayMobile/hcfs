#include "hcfscurl.h"
#include "mock_params.h"

//#ifdef __CURL_CURL_H
//#undef __CURL_TYPECHECK_GCC_H

//#define curl_easy_setopt my_curl
//URLcode curl_easy_setopt(CURL *handle, CURLoption option, int parameter)
#undef curl_easy_setopt
CURLcode curl_easy_setopt(CURL *handle, CURLoption option, ...)
{
}

struct curl_slist *curl_slist_append(struct curl_slist * list, const char * string )
{
}

void curl_slist_free_all(struct curl_slist * list)
{
}

int write_log(int level, char *format, ...)
{
	return 0;
}

//#endif

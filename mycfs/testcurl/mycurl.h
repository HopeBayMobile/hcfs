#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#define URL_HEADER "X-Storage-Url:"
#define AUTH_HEADER "X-Auth-Token:"

char url_string[200];
char container_string[200];
char auth_string[200];

CURL *curl;
CURLcode res;

size_t read_header_auth(void *bufptr, size_t size, size_t nmemb, void *tempbuffer);
void swift_get_auth_info(char *swift_user,char *swift_pass, char *swift_url);
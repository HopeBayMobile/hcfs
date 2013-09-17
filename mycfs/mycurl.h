#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#define URL_HEADER "X-Storage-Url:"
#define AUTH_HEADER "X-Auth-Token:"

#define MY_ACCOUNT "mycfs"
#define MY_USER "mycfs"
#define MY_PASS "mycfs"
#define MY_URL "10.10.0.1:8080"

char url_string[200];
char container_string[200];
char auth_string[200];

CURL *curl;
CURLcode res;

size_t read_header_auth(void *bufptr, size_t size, size_t nmemb, void *tempbuffer);
int swift_get_auth_info(char *swift_user,char *swift_pass, char *swift_url);
int init_swift_backend();
void destroy_swift_backend();
int swift_list_container();
int swift_put_object(FILE *fptr, char *objname);
int swift_get_object(FILE *fptr, char *objname);

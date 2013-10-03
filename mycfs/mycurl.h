#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <semaphore.h>
#define URL_HEADER "X-Storage-Url:"
#define AUTH_HEADER "X-Auth-Token:"

#define MY_ACCOUNT "mycfs"
#define MY_USER "mycfs"
#define MY_PASS "mycfs"
#define MY_URL "10.10.0.1:8080"


#define MAX_CURL_HANDLE 16

char url_string[200];
char container_string[200];
char auth_string[200];

typedef struct {
    CURL *curl;
 } CURL_HANDLE;

CURL_HANDLE download_curl_handles[MAX_CURL_HANDLE];
short curl_handle_mask[MAX_CURL_HANDLE];
sem_t download_curl_sem;

size_t read_header_auth(void *bufptr, size_t size, size_t nmemb, void *tempbuffer);
int swift_get_auth_info(char *swift_user,char *swift_pass, char *swift_url, CURL *curl);
int init_swift_backend(CURL_HANDLE *curl_handle);
void destroy_swift_backend(CURL *curl);
int swift_list_container(CURL *curl);
int swift_put_object(FILE *fptr, char *objname, CURL *curl);
int swift_get_object(FILE *fptr, char *objname, CURL *curl);
int swift_reauth();

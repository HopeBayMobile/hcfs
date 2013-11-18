#include <semaphore.h>


#define MY_ACCOUNT "mycfs"
#define MY_USER "mycfs"
#define MY_PASS "mycfs"
#define MY_URL "10.10.0.1:8080"

 
#define MAX_CURL_HANDLE 16

typedef struct {
    CURL *curl;
  } CURL_HANDLE;

CURL_HANDLE download_curl_handles[MAX_CURL_HANDLE];
short curl_handle_mask[MAX_CURL_HANDLE];
sem_t download_curl_sem;
int swift_get_auth_info(char *swift_user,char *swift_pass, char *swift_url, CURL *curl);
int init_swift_backend(CURL_HANDLE *curl_handle);
void destroy_swift_backend(CURL *curl);
int swift_list_container(CURL *curl);
int swift_put_object(FILE *fptr, char *objname, CURL *curl);
int swift_get_object(FILE *fptr, char *objname, CURL *curl);
int swift_reauth();
int swift_delete_object(char *objname, CURL *curl);


#include <semaphore.h>
#include <curl/curl.h>


#define MY_ACCOUNT "mycfs"
#define MY_USER "mycfs"
#define MY_PASS "mycfs"
#define MY_URL "https://10.10.0.1:8080"

 
#define MAX_DOWNLOAD_CURL_HANDLE 16

typedef struct {
    CURL *curl;
    char id[256];  /*A short name representing the unique identity of the handle*/
  } CURL_HANDLE;

char swift_auth_string[1024];
char swift_url_string[1024];

CURL_HANDLE download_curl_handles[MAX_DOWNLOAD_CURL_HANDLE];
short curl_handle_mask[MAX_DOWNLOAD_CURL_HANDLE];
sem_t download_curl_control_sem;
sem_t download_curl_sem;

int hcfs_get_auth_swift(char *swift_user,char *swift_pass, char *swift_url, CURL_HANDLE *curl_handle);
int hcfs_init_swift_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_swift_backend(CURL *curl);
int hcfs_swift_list_container(CURL_HANDLE *curl_handle);
int hcfs_swift_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_swift_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_swift_reauth();
int hcfs_swift_delete_object(char *objname, CURL_HANDLE *curl_handle);
void do_block_sync(ino_t this_inode, long block_no, CURL_HANDLE *curl_handle, char *filename);
void do_meta_sync(ino_t this_inode, CURL_HANDLE *curl_handle, char *filename);

void fetch_from_cloud(FILE *fptr, ino_t this_inode, long block_no);

#ifndef GW20_HCFS_HCFSCURL_H_
#define GW20_HCFS_HCFSCURL_H_

#define S3 1
#define SWIFT 0

#define CURRENT_BACKEND SWIFT

#define MY_ACCOUNT "hopebay"
#define MY_USER "hopebay"
#define MY_PASS "hopebaycloud"
#define MY_URL "https://192.168.70.31:8080"

#define S3_ACCESS "SE41NTAxMzIwNDEzOTQ1Mjk5NTkwNzI"
#define S3_SECRET "NWUzOTdjNjRjOGI1NDM2ZmI4MzFlOTgxMjdkNmMyNWM"
#define S3_URL "https://s3.hicloud.net.tw"
#define S3_BUCKET "testgateway"
#define S3_BUCKET_URL "https://testgateway.s3.hicloud.net.tw"

 
#define MAX_DOWNLOAD_CURL_HANDLE 16

typedef struct {
    CURL *curl;
    char id[256];  /*A short name representing the unique identity of the handle*/
    int curl_backend;
  } CURL_HANDLE;

char swift_auth_string[1024];
char swift_url_string[1024];

CURL_HANDLE download_curl_handles[MAX_DOWNLOAD_CURL_HANDLE];
short curl_handle_mask[MAX_DOWNLOAD_CURL_HANDLE];
sem_t download_curl_control_sem;
sem_t download_curl_sem;

/* Swift collections */

int hcfs_get_auth_swift(char *swift_user,char *swift_pass, char *swift_url, CURL_HANDLE *curl_handle);
int hcfs_init_swift_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_swift_backend(CURL *curl);
int hcfs_swift_list_container(CURL_HANDLE *curl_handle);
int hcfs_swift_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_swift_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_swift_reauth(CURL_HANDLE *curl_handle);
int hcfs_swift_delete_object(char *objname, CURL_HANDLE *curl_handle);


/* S3 collections */
int hcfs_init_S3_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_S3_backend(CURL *curl);
int hcfs_S3_list_container(CURL_HANDLE *curl_handle);
int hcfs_S3_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_S3_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_S3_delete_object(char *objname, CURL_HANDLE *curl_handle);
int hcfs_S3_reauth(CURL_HANDLE *curl_handle);

/* Generic */
int hcfs_init_backend(CURL_HANDLE *curl_handle);
void hcfs_destroy_backend(CURL *curl);
int hcfs_list_container(CURL_HANDLE *curl_handle);
int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle);
int hcfs_delete_object(char *objname, CURL_HANDLE *curl_handle);

#endif  /* GW20_HCFS_HCFSCURL_H_ */

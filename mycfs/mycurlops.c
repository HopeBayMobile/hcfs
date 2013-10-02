#include "mycurl.h"

typedef struct {
    FILE *fptr;
    long object_size;
    long remaining_size;
 } object_put_control;

size_t read_header_auth(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  if (!strncmp(bufptr,URL_HEADER,strlen(URL_HEADER)))
   {
    strcpy(url_string,&bufptr[strlen(URL_HEADER)+1]);
    if (url_string[strlen(url_string)-1]=='\n')
     url_string[strlen(url_string)-2]=0;
   }
  else if (!strncmp(bufptr,AUTH_HEADER,strlen(AUTH_HEADER)))
   {
    strcpy(auth_string,bufptr);
    if (auth_string[strlen(auth_string)-1]=='\n')
     auth_string[strlen(auth_string)-1]=0;
   }


  return size*nmemb;
 }
char auth_header_buf[1000];
int swift_get_auth_info(char *swift_user,char *swift_pass, char *swift_url, CURL *curl)
 {
  char userstring[1000];
  char passstring[1000];
  char urlstring[1000];
  struct curl_slist *chunk=NULL;
  CURLcode res;

  //printf("%s, %s, %s\n", swift_user, swift_pass, swift_url);
  sprintf(userstring,"X-Storage-User: %s",swift_user);
  sprintf(passstring,"X-Storage-Pass: %s",swift_pass);
  sprintf(urlstring,"https://%s/auth/v1.0",swift_url);

  chunk=curl_slist_append(chunk, userstring);
  chunk=curl_slist_append(chunk, passstring);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, read_header_auth);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, auth_header_buf);

  curl_easy_setopt(curl,CURLOPT_URL, urlstring);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    return -1;
   }

  curl_slist_free_all(chunk);

  return 0;
 }

int init_swift_backend(CURL *curl)
 {
  char account_user_string[1000];
  int ret_code;
  curl = curl_easy_init();

  if (curl)
   {
    //struct curl_slist *chunk=NULL;

    sprintf(account_user_string,"%s:%s",MY_ACCOUNT,MY_USER);

    ret_code = swift_get_auth_info(account_user_string, MY_PASS, MY_URL, curl);

    return ret_code;
   }
  return -1;
 }
int swift_reauth()
 {
  char account_user_string[1000];
  int ret_code;
  CURL *curl;
  curl = curl_easy_init();

  if (curl)
   {
    //struct curl_slist *chunk=NULL;

    sprintf(account_user_string,"%s:%s",MY_ACCOUNT,MY_USER);

    ret_code = swift_get_auth_info(account_user_string, MY_PASS, MY_URL, curl);

    destroy_swift_backend(curl);

    return ret_code;
   }
  return -1;
 }
void destroy_swift_backend(CURL *curl)
 {
  curl_easy_cleanup(curl);
  return;
 }
size_t read_header_dummy(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  return size*nmemb;
 }

size_t write_function(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  FILE *fptr;
  long num_ele_read;

  fptr=fopen("tempstore","a+");

  num_ele_read=fwrite(bufptr,size,nmemb,fptr);

  fclose(fptr);
  return size*num_ele_read;
 }

size_t read_file_function(void *ptr, size_t size, size_t nmemb, void *put_control1)
 {
/*TODO: Consider if it is possible for the actual file size to be smaller than object size due to truncating*/
  long total_size;
  FILE *fptr;
  size_t actual_to_read;
  object_put_control *put_control;

  put_control = (object_put_control *) put_control1; 

  if (put_control->remaining_size <=0)
   return 0;

  fptr=put_control->fptr;
  if ((size*nmemb) > put_control->remaining_size)
   {
    actual_to_read = put_control->remaining_size;
   }
  else
   actual_to_read = size * nmemb;

//  fprintf(stderr,"Debug swift_put_object: start read %ld %ld\n",size,nmemb);
  total_size=fread(ptr,1,actual_to_read,fptr);
  put_control->remaining_size -= total_size;
//  fprintf(stderr,"Debug swift_put_object: end read %ld\n",total_size);

  return total_size;
 }

size_t write_file_function(void *ptr, size_t size, size_t nmemb, void *fstream)
 {
  long total_size;

  total_size=fwrite(ptr,size,nmemb,fstream);

  return total_size*size;
 }


char http_header_buffer[1000];
char http_write_buffer[1000];
char http_read_buffer[1000];

int swift_list_container(CURL *curl)
 {
  struct curl_slist *chunk=NULL;
  CURLcode res;

  chunk=NULL;

  sprintf(container_string,"%s/%s_private_container",url_string,MY_USER);
  chunk=curl_slist_append(chunk, auth_string);
  chunk=curl_slist_append(chunk, "Expect:");
  chunk=curl_slist_append(chunk, "Content-Length:");

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, read_header_dummy);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, http_header_buffer);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, http_write_buffer);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    return -1;
   }

  curl_slist_free_all(chunk);
  return 0;
 }

int swift_put_object(FILE *fptr, char *objname, CURL *curl)
 {
  struct curl_slist *chunk=NULL;
  long objsize;
  object_put_control put_control;
  CURLcode res;

  chunk=NULL;

  sprintf(container_string,"%s/%s_private_container/%s",url_string,MY_USER,objname);
  chunk=curl_slist_append(chunk, auth_string);
  chunk=curl_slist_append(chunk, "Expect:");
  fseek(fptr,0,SEEK_END);
  objsize=ftell(fptr);
  fseek(fptr,0,SEEK_SET);
  put_control.fptr=fptr;
  put_control.object_size = objsize;
  put_control.remaining_size = objsize;

  //printf("Debug swift_put_object: object size is %ld\n",objsize);

  if (objsize < 0)
   return -1;

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READDATA, (void *) &put_control);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, objsize);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  //printf("Debug swift_put_object: test1\n",objsize);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    return -1;
   }

  //printf("Debug swift_put_object: test2\n",objsize);
  curl_slist_free_all(chunk);
  //printf("Debug swift_put_object: test3\n",objsize);
  return 0;
 }
int swift_get_object(FILE *fptr, char *objname, CURL *curl)
 {
  struct curl_slist *chunk=NULL;
  long objsize;
  CURLcode res;

  chunk=NULL;

  sprintf(container_string,"%s/%s_private_container/%s",url_string,MY_USER,objname);
  chunk=curl_slist_append(chunk, auth_string);
  chunk=curl_slist_append(chunk, "Expect:");
  fseek(fptr,0,SEEK_END);
  objsize=ftell(fptr);
  fseek(fptr,0,SEEK_SET);
  if (objsize < 0)
   return -1;

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fptr);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    return -1;
   }

  curl_slist_free_all(chunk);
  return 0;
 }


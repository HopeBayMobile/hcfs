/* Implemented routines to parse http return code to distinguish errors from normal ops*/
/*TODO: Need to implement retry mechanisms and also http timeout */

#include "hcfscurl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *fptr;
    long object_size;
    long remaining_size;
 } object_put_control;

size_t write_file_function(void *ptr, size_t size, size_t nmemb, void *fstream)
 {
  long total_size;

  total_size=fwrite(ptr,size,nmemb,fstream);

  return total_size*size;
 }

int parse_auth_header(FILE *fptr)
 {
  char httpcode[20],retcode[20],retstatus[20];
  char temp_string[1024], temp_string2[1024];
  int ret_val,retcodenum;

  fseek(fptr,0,SEEK_SET);
  ret_val = fscanf(fptr,"%s %s %s\n",httpcode,retcode,retstatus);
  if (ret_val < 3)
   return -1;

  retcodenum=atoi(retcode);

  if ((retcodenum<200) || (retcodenum>299))
   return retcodenum;

  ret_val = fscanf(fptr,"%s %s\n",temp_string,swift_url_string);

  if (ret_val < 2)
   return -1;

  ret_val = fscanf(fptr,"%s %s\n",temp_string,temp_string2);
  if (ret_val < 2)
   return -1;

  sprintf(swift_auth_string,"%s %s",temp_string,temp_string2);

  return retcodenum;
 }

int parse_list_header(FILE *fptr)
 {
  char httpcode[20],retcode[20],retstatus[20];
  char temp_string[1024], temp_string2[1024];
  int ret_val,retcodenum, total_objs;

  fseek(fptr,0,SEEK_SET);
  ret_val = fscanf(fptr,"%s %s %s\n",httpcode,retcode,retstatus);
  if (ret_val < 3)
   return -1;

  retcodenum=atoi(retcode);

  if ((retcodenum<200) || (retcodenum>299))
   return retcodenum;

  ret_val = fscanf(fptr,"X-Container-Object-Count: %s\n",temp_string);

  if (ret_val<1)
   return -1;
  
  total_objs = atoi(temp_string);

  printf("total objects %d\n",total_objs);

  return retcodenum;
 }

int parse_http_header_retcode(FILE *fptr)
 {
  char httpcode[20],retcode[20],retstatus[20];
  int ret_val,retcodenum;

  fseek(fptr,0,SEEK_SET);
  ret_val = fscanf(fptr,"%s %s %s\n",httpcode,retcode,retstatus);
  if (ret_val < 3)
   return -1;

  retcodenum=atoi(retcode);

  return retcodenum;
 }


void dump_list_body(FILE *fptr)
 {
  char temp_string[1024];
  int ret_val;

  fseek(fptr,0,SEEK_SET);
  while(!feof(fptr))
   {
    ret_val = fscanf(fptr,"%s\n",temp_string);
    if (ret_val < 1)
     break;
//    printf("%s\n",temp_string);
   }
  return;
 }

int hcfs_get_auth_swift(char *swift_user,char *swift_pass, char *swift_url, CURL_HANDLE *curl_handle)
 {
  struct curl_slist *chunk=NULL;
  CURLcode res;
  char auth_url[200];
  char user_string[1024];
  char pass_string[1024];
  int ret_val;
  FILE *fptr;
  CURL *curl;
  char filename[100];

  sprintf(filename,"/run/shm/swiftauth%s.tmp",curl_handle->id);
  curl = curl_handle->curl;

  fptr=fopen(filename,"w+");
  chunk=NULL;

  sprintf(auth_url,"%s/auth/v1.0",swift_url);
  sprintf(user_string,"X-Storage-User: %s",swift_user);
  sprintf(pass_string,"X-Storage-Pass: %s",swift_pass);
  chunk=curl_slist_append(chunk, user_string);
  chunk=curl_slist_append(chunk, pass_string);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, fptr);
  curl_easy_setopt(curl,CURLOPT_URL, auth_url);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);

  res = curl_easy_perform(curl);

  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(fptr);
    unlink(filename);
    curl_slist_free_all(chunk);

    return -1;
   }

  ret_val = parse_auth_header(fptr);

  /*TODO: add retry routines somewhere for failed attempts*/  

  fclose(fptr);
  unlink(filename);

  curl_slist_free_all(chunk);

  return ret_val;
 }

 
int hcfs_init_swift_backend(CURL_HANDLE *curl_handle)
 {
  char account_user_string[1000];
  int ret_code;
  curl_handle->curl = curl_easy_init();

  if (curl_handle->curl)
   {
    //struct curl_slist *chunk=NULL;

    sprintf(account_user_string,"%s:%s",MY_ACCOUNT,MY_USER);

    ret_code = hcfs_get_auth_swift(account_user_string, MY_PASS, MY_URL, curl_handle);

    return ret_code;
   }
  return -1;
 }

int hcfs_swift_reauth()
 {
  char account_user_string[1000];
  int ret_code;
  CURL *curl;

  curl = curl_easy_init();

  if (curl)
   {
    //struct curl_slist *chunk=NULL;

    sprintf(account_user_string,"%s:%s",MY_ACCOUNT,MY_USER);

    ret_code = hcfs_get_auth_swift(account_user_string, MY_PASS, MY_URL, curl);

    hcfs_destroy_swift_backend(curl);

    return ret_code;
   }
  return -1;
 }
void hcfs_destroy_swift_backend(CURL *curl)
 {
  curl_easy_cleanup(curl);
  return;
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

  //fprintf(stderr,"Debug swift_put_object: start read %ld %ld\n",size,nmemb);
  total_size=fread(ptr,1,actual_to_read,fptr);
  put_control->remaining_size -= total_size;
  //fprintf(stderr,"Debug swift_put_object: end read %ld\n",total_size);

  return total_size;
 }


int hcfs_swift_list_container(CURL_HANDLE *curl_handle)
 {
/*TODO: How to actually export the list of objects to other functions*/
  struct curl_slist *chunk=NULL;
  CURLcode res;
  char container_string[200];
  FILE *swift_list_header_fptr,*swift_list_body_fptr;
  CURL *curl;
  char header_filename[100],body_filename[100];
  int ret_val;

  sprintf(header_filename,"/run/shm/swiftlisthead%s.tmp",curl_handle->id);
  sprintf(body_filename,"/run/shm/swiftlistbody%s.tmp",curl_handle->id);
  curl = curl_handle->curl;

  swift_list_header_fptr=fopen(header_filename,"w+");
  swift_list_body_fptr=fopen(body_filename,"w+");

  chunk=NULL;

  sprintf(container_string,"%s/%s_private_container",swift_url_string,MY_USER);
  chunk=curl_slist_append(chunk, swift_auth_string);
  chunk=curl_slist_append(chunk, "Expect:");
  chunk=curl_slist_append(chunk, "Content-Length:");

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, swift_list_body_fptr);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);

  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(swift_list_header_fptr);
    unlink(header_filename);
    fclose(swift_list_body_fptr);
    unlink(body_filename);
    curl_slist_free_all(chunk);
    return -1;
   }

  ret_val = parse_list_header(swift_list_header_fptr);

  if ((ret_val>=200) && (ret_val < 300))
   dump_list_body(swift_list_body_fptr);
  /*TODO: add retry routines somewhere for failed attempts*/  

  fclose(swift_list_header_fptr);
  unlink(header_filename);
  fclose(swift_list_body_fptr);
  unlink(body_filename);

  curl_slist_free_all(chunk);

  return ret_val;
 }
int hcfs_swift_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
 {
  struct curl_slist *chunk=NULL;
  long objsize;
  object_put_control put_control;
  CURLcode res;
  char container_string[200];
  FILE *swift_list_header_fptr;
  CURL *curl;
  char header_filename[100];
  int ret_val;

  sprintf(header_filename,"/run/shm/swiftputhead%s.tmp",curl_handle->id);
  curl = curl_handle->curl;

  swift_list_header_fptr=fopen(header_filename,"w+");

  chunk=NULL;

  sprintf(container_string,"%s/%s_private_container/%s",swift_url_string,MY_USER,objname);
  chunk=curl_slist_append(chunk, swift_auth_string);
  chunk=curl_slist_append(chunk, "Expect:");
  fseek(fptr,0,SEEK_END);
  objsize=ftell(fptr);
  fseek(fptr,0,SEEK_SET);
  put_control.fptr=fptr;
  put_control.object_size = objsize;
  put_control.remaining_size = objsize;

  printf("Debug swift_put_object: object size is %ld\n",objsize);
  printf("Put to %s, auth %s\n",container_string,swift_auth_string);

  if (objsize < 0)
   {
    fclose(swift_list_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);

    return -1;
   }

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READDATA, (void *) &put_control);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, objsize);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  //printf("Debug swift_put_object: test1\n",objsize);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(swift_list_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);

    return -1;
   }

  //printf("Debug swift_put_object: test2\n",objsize);
  curl_slist_free_all(chunk);
  //printf("Debug swift_put_object: test3\n",objsize);
  ret_val = parse_http_header_retcode(swift_list_header_fptr);
  fclose(swift_list_header_fptr);
  unlink(header_filename);

  return ret_val;
 }
int hcfs_swift_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
 {
  struct curl_slist *chunk=NULL;
  long objsize;
  CURLcode res;
  char container_string[200];

  FILE *swift_list_header_fptr;
  CURL *curl;
  char header_filename[100];
  int ret_val;

  sprintf(header_filename,"/run/shm/swiftgethead%s.tmp",curl_handle->id);
  curl = curl_handle->curl;

  swift_list_header_fptr=fopen(header_filename,"w+");

  chunk=NULL;

  sprintf(container_string,"%s/%s_private_container/%s",swift_url_string,MY_USER,objname);
  chunk=curl_slist_append(chunk, swift_auth_string);
  chunk=curl_slist_append(chunk, "Expect:");
  fseek(fptr,0,SEEK_END);
  objsize=ftell(fptr);
  fseek(fptr,0,SEEK_SET);
  if (objsize < 0)
   {
    fclose(swift_list_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);

    return -1;
   }

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fptr);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(swift_list_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);
    return -1;
   }
 
  curl_slist_free_all(chunk);
  ret_val = parse_http_header_retcode(swift_list_header_fptr);
  fclose(swift_list_header_fptr);
  unlink(header_filename);

  return ret_val;
 }
int hcfs_swift_delete_object(char *objname, CURL_HANDLE *curl_handle)
 {
  struct curl_slist *chunk=NULL;
  long objsize;
  CURLcode res;
  char container_string[200];
  char delete_command[10];

  FILE *swift_list_header_fptr;
  CURL *curl;
  char header_filename[100];
  int ret_val;

  sprintf(header_filename,"/run/shm/swiftdeletehead%s.tmp",curl_handle->id);
  curl = curl_handle->curl;

  swift_list_header_fptr=fopen(header_filename,"w+");
  strcpy(delete_command,"DELETE");
  chunk=NULL;

  sprintf(container_string,"%s/%s_private_container/%s",swift_url_string,MY_USER,objname);
  chunk=curl_slist_append(chunk, swift_auth_string);
  chunk=curl_slist_append(chunk, "Expect:");

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, delete_command);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, swift_list_header_fptr);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(swift_list_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);
    return -1;
   }

  curl_slist_free_all(chunk);
  ret_val = parse_http_header_retcode(swift_list_header_fptr);
  fclose(swift_list_header_fptr);
  unlink(header_filename);

  return ret_val;
 }

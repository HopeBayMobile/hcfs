/* Implemented routines to parse http return code to distinguish errors from normal ops*/
/*TODO: Need to implement retry mechanisms and also http timeout */
/*TODO: Continue to test S3 deletion problem */

#include "hcfscurl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <openssl/hmac.h>
#include <openssl/engine.h>

typedef struct {
    FILE *fptr;
    off_t object_size;
    off_t remaining_size;
 } object_put_control;

size_t write_file_function(void *ptr, size_t size, size_t nmemb, void *fstream)
 {
  size_t total_size;

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
  ret_val = fscanf(fptr,"%s %s",httpcode,retcode);
  if (ret_val < 2)
   return -1;

  fgets(retstatus, 19, fptr);
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
int parse_S3_list_header(FILE *fptr)
 {
  char httpcode[20],retcode[20],retstatus[20];
  char temp_string[1024], temp_string2[1024];
  int ret_val,retcodenum, total_objs;

  fseek(fptr,0,SEEK_SET);
  ret_val = fscanf(fptr,"%s %s",httpcode,retcode);
  if (ret_val < 2)
   return -1;

  fgets(retstatus, 19, fptr);
  retcodenum=atoi(retcode);

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
    printf("%s\n",temp_string);
   }
  return;
 }
void dump_S3_list_body(FILE *fptr)
 {
  char temp_string[1024];
  int ret_val;

  fseek(fptr,0,SEEK_SET);
  while(!feof(fptr))
   {
    ret_val = fread(temp_string,1, 512, fptr);
    temp_string[ret_val]=0;
    if (ret_val < 1)
     break;
    printf("%s",temp_string);
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
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
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
int hcfs_init_S3_backend(CURL_HANDLE *curl_handle)
 {
  char account_user_string[1000];
  int ret_code;
  curl_handle->curl = curl_easy_init();

  if (curl_handle->curl)
   {
    //struct curl_slist *chunk=NULL;

    return 200;
   }
  return -1;
 }
int hcfs_swift_reauth(CURL_HANDLE *curl_handle)
 {
  char account_user_string[1000];
  int ret_code;

  if (curl_handle->curl != NULL)
   hcfs_destroy_swift_backend(curl_handle->curl);

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
void hcfs_destroy_swift_backend(CURL *curl)
 {
  curl_easy_cleanup(curl);
  return;
 }
void hcfs_destroy_S3_backend(CURL *curl)
 {
  curl_easy_cleanup(curl);
  return;
 }

size_t read_file_function(void *ptr, size_t size, size_t nmemb, void *put_control1)
 {
  /*TODO: Consider if it is possible for the actual file size to be smaller than object size due to truncating*/
  size_t total_size;
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
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
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
  off_t objsize;
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

  //printf("Debug swift_put_object: object size is %ld\n",objsize);
  //printf("Put to %s, auth %s\n",container_string,swift_auth_string);

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
  curl_easy_setopt(curl, CURLOPT_INFILESIZE, objsize);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
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
  off_t objsize;
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
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
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
  off_t objsize;
  CURLcode res;
  char container_string[200];
  char delete_command[10];

  FILE *swift_list_header_fptr;
  CURL *curl;
  char header_filename[100];
  int ret_val;

  //printf("Debug swift_delete_object: object is %s\n",objname);

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

void convert_currenttime(unsigned char *date_string)
 {
  char current_time[100];
  char wday[5];
  char month[5];
  char mday[5];
  char timestr[20];
  char year[6];
  time_t tmptime;

  tmptime=time(NULL);
  strcpy(current_time, asctime(gmtime(&tmptime)));

  printf("current time %s\n",current_time);

  sscanf(current_time,"%s %s %s %s %s\n", wday, month, mday, timestr, year);

  sprintf(date_string, "%s, %s %s %s %s GMT", wday, mday, month, year, timestr);

  printf("converted string %s\n", date_string);

  return;
 }

void compute_hmac_sha1(unsigned char *input_str, unsigned char *output_str, unsigned char *key, int *outputlen)
 {
  unsigned char finalhash[4096];
  int len_finalhash;
  HMAC_CTX myctx;
  int count;

  printf("key: %s\n",key);
  printf("input: %s\n", input_str);
  printf("%d, %d\n",strlen(key),strlen(input_str));
  HMAC_CTX_init(&myctx);

  HMAC_Init_ex(&myctx,key,strlen(key),EVP_sha1(), NULL);
  HMAC_Update(&myctx,input_str,strlen(input_str));
  HMAC_Final(&myctx, finalhash, &len_finalhash);
  HMAC_CTX_cleanup(&myctx);

  memcpy(output_str,finalhash,len_finalhash);
  output_str[len_finalhash]=0;
  *outputlen=len_finalhash;

  for(count=0;count<len_finalhash;count++)
   printf("%02X",finalhash[count]);
  printf("\n");

  return;
 }
void generate_S3_sig(char *method, char *date_string, char *sig_string, char *resource_string)
 {
  unsigned char sig_temp1[4096],sig_temp2[4096];
  int len_signature,hashlen;

  convert_currenttime(date_string);
  sprintf(sig_temp1,"%s\n\n\n%s\n/%s",method,date_string,resource_string);
  printf("sig temp1: %s\n",sig_temp1);
  compute_hmac_sha1(sig_temp1,sig_temp2,S3_SECRET,&hashlen);
  printf("sig temp2: %s\n",sig_temp2);
  b64encode_str(sig_temp2,sig_string,&len_signature,hashlen);

  printf("final sig: %s, %d\n",sig_string,hashlen);

  return;
 }

int hcfs_S3_list_container(CURL_HANDLE *curl_handle)
 {
/*TODO: How to actually export the list of objects to other functions*/
  struct curl_slist *chunk=NULL;
  CURLcode res;
  FILE *S3_list_header_fptr,*S3_list_body_fptr;
  CURL *curl;
  char header_filename[100],body_filename[100];

  unsigned char date_string[100];
  char date_string_header[100];
  unsigned char AWS_auth_string[200];
  unsigned char S3_signature[200];
  unsigned char resource[200];
  int ret_val;

  sprintf(header_filename,"/run/shm/S3listhead%s.tmp",curl_handle->id);
  sprintf(body_filename,"/run/shm/S3listbody%s.tmp",curl_handle->id);
  sprintf(resource,"%s/",S3_BUCKET);

  curl = curl_handle->curl;

  S3_list_header_fptr=fopen(header_filename,"w+");
  S3_list_body_fptr=fopen(body_filename,"w+");

  generate_S3_sig("GET", date_string,S3_signature,resource);

  sprintf(date_string_header, "date: %s", date_string);
  sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS, S3_signature);

  printf("%s\n",AWS_auth_string);

  chunk=NULL;

  chunk=curl_slist_append(chunk, "Expect:");
  chunk=curl_slist_append(chunk, "Content-Length:");
  chunk=curl_slist_append(chunk, date_string_header);
  chunk=curl_slist_append(chunk, AWS_auth_string);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_list_header_fptr);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, S3_list_body_fptr);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_function);
  curl_easy_setopt(curl,CURLOPT_URL, S3_BUCKET_URL);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);

  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(S3_list_header_fptr);
    unlink(header_filename);
    fclose(S3_list_body_fptr);
    unlink(body_filename);
    curl_slist_free_all(chunk);
    return -1;
   }

  ret_val = parse_S3_list_header(S3_list_header_fptr);

  if ((ret_val>=200) && (ret_val < 300))
   dump_S3_list_body(S3_list_body_fptr);
  /*TODO: add retry routines somewhere for failed attempts*/  

  printf("return val is: %d\n",ret_val);

  fclose(S3_list_header_fptr);
  unlink(header_filename);
  fclose(S3_list_body_fptr);
  unlink(body_filename);

  curl_slist_free_all(chunk);

  return ret_val;
 }


int hcfs_init_backend(CURL_HANDLE *curl_handle)
 {
  int ret_val;
  switch (CURRENT_BACKEND)
   {
    case SWIFT:

     ret_val = hcfs_init_swift_backend(curl_handle);
     while ((ret_val < 200) || (ret_val > 299))
      {
       if (curl_handle->curl !=NULL)
        hcfs_destroy_swift_backend(curl_handle->curl);
       ret_val = hcfs_init_swift_backend(curl_handle);
      }

     break;
    case S3:
     ret_val = hcfs_init_S3_backend(curl_handle);
     break;
    default:
     ret_val = -1;
     break;
   }

  return ret_val;
 }
     

void hcfs_destroy_backend(CURL *curl)
 {
  switch (CURRENT_BACKEND)
   {
    case SWIFT:
     hcfs_destroy_swift_backend(curl);
     break;
    case S3:
     hcfs_destroy_S3_backend(curl);
     break;
    default:
     break;
   }

  return;
 }
/* TODO: Fix handling in reauthing in SWIFT. Now will try to reauth for any HTTP error*/
/* TODO: nothing is actually returned in list container. FIX THIS*/

int hcfs_list_container(CURL_HANDLE *curl_handle)
 {
  int ret_val;

  switch (CURRENT_BACKEND)
   {
    case SWIFT:
     ret_val = hcfs_swift_list_container(curl_handle);
     while ((ret_val < 200) || (ret_val > 299))
      {
       ret_val = hcfs_swift_reauth(curl_handle);
       if ((ret_val >= 200) && (ret_val <=299))
        {
         ret_val = hcfs_swift_list_container(curl_handle);
        }
      }
     break;
    case S3:
     ret_val = hcfs_S3_list_container(curl_handle);
     break;
    default:
     ret_val = -1;
     break;
   }
  return ret_val;

 }
/* TODO: Fix handling in reauthing in SWIFT. Now will try to reauth for any HTTP error*/

int hcfs_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
 {
  int ret_val;

  switch (CURRENT_BACKEND)
   {
    case SWIFT:
     ret_val = hcfs_swift_put_object(fptr,objname, curl_handle);
     while ((ret_val < 200) || (ret_val > 299))
      {
       ret_val = hcfs_swift_reauth(curl_handle);
       if ((ret_val >= 200) && (ret_val <=299))
        {
         fseek(fptr,0,SEEK_SET);
         ret_val = hcfs_swift_put_object(fptr,objname, curl_handle);
        }
      }
     break;
    case S3:
     ret_val = hcfs_S3_put_object(fptr,objname, curl_handle);
     break;
    default:
     ret_val = -1;
     break;
   }

  return ret_val;
 }
/* TODO: Fix handling in reauthing in SWIFT. Now will try to reauth for any HTTP error*/

int hcfs_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
 {

  int status;

  switch (CURRENT_BACKEND)
   {
    case SWIFT:

     status=hcfs_swift_get_object(fptr,objname,curl_handle);

     while ((status< 200) || (status > 299))
       status = hcfs_swift_reauth(curl_handle);
     break;

    case S3:
     status = hcfs_S3_get_object(fptr,objname,curl_handle);
     break;
    default:
     status = -1;
     break;
   }
  return status;
 }


/* TODO: Fix handling in reauthing in SWIFT. Now will try to reauth for any HTTP error*/
int hcfs_delete_object(char *objname, CURL_HANDLE *curl_handle)
 {
  int ret_val;

  switch (CURRENT_BACKEND)
   {
    case SWIFT:
     ret_val = hcfs_swift_delete_object(objname, curl_handle);
     while (((ret_val < 200) || (ret_val > 299)) && (ret_val !=404))
      {
       ret_val = hcfs_swift_reauth(curl_handle);
       if ((ret_val >= 200) && (ret_val <=299))
        {
         ret_val = hcfs_swift_delete_object(objname, curl_handle);
        }
      }
     break;
    case S3:
     ret_val = hcfs_S3_delete_object(objname, curl_handle);
     break;
    default:
     ret_val = -1;
     break;
   }
  return ret_val;
 }

int hcfs_S3_put_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
 {
  struct curl_slist *chunk=NULL;
  off_t objsize;
  object_put_control put_control;
  CURLcode res;
  char container_string[200];
  FILE *S3_header_fptr;
  CURL *curl;
  char header_filename[100];

  unsigned char date_string[100];
  char date_string_header[100];
  unsigned char AWS_auth_string[200];
  unsigned char S3_signature[200];
  int ret_val;
  unsigned char resource[200];


  sprintf(header_filename,"/run/shm/s3puthead%s.tmp",curl_handle->id);
  sprintf(resource,"%s/%s",S3_BUCKET,objname);
  curl = curl_handle->curl;

  S3_header_fptr=fopen(header_filename,"w+");

  generate_S3_sig("PUT", date_string,S3_signature,resource);

  sprintf(date_string_header, "date: %s", date_string);
  sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS, S3_signature);

  printf("%s\n",AWS_auth_string);

  chunk=NULL;

  sprintf(container_string,"%s/%s",S3_BUCKET_URL,objname);
  chunk=curl_slist_append(chunk, "Expect:");
  chunk=curl_slist_append(chunk, date_string_header);
  chunk=curl_slist_append(chunk, AWS_auth_string);

  fseek(fptr,0,SEEK_END);
  objsize=ftell(fptr);
  fseek(fptr,0,SEEK_SET);
  put_control.fptr=fptr;
  put_control.object_size = objsize;
  put_control.remaining_size = objsize;

  //printf("Debug swift_put_object: object size is %ld\n",objsize);

  if (objsize < 0)
   {
    fclose(S3_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);

    return -1;
   }

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(curl, CURLOPT_READDATA, (void *) &put_control);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE, objsize);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_file_function);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_header_fptr);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  //printf("Debug swift_put_object: test1\n",objsize);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(S3_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);

    return -1;
   }

  //printf("Debug swift_put_object: test2\n",objsize);
  curl_slist_free_all(chunk);
  //printf("Debug swift_put_object: test3\n",objsize);
  ret_val = parse_http_header_retcode(S3_header_fptr);
  fclose(S3_header_fptr);
  unlink(header_filename);

  return ret_val;
 }
int hcfs_S3_get_object(FILE *fptr, char *objname, CURL_HANDLE *curl_handle)
 {
  struct curl_slist *chunk=NULL;
  off_t objsize;
  CURLcode res;
  char container_string[200];

  FILE *S3_list_header_fptr;
  CURL *curl;
  char header_filename[100];
  int ret_val;

  unsigned char date_string[100];
  char date_string_header[100];
  unsigned char AWS_auth_string[200];
  unsigned char S3_signature[200];
  unsigned char resource[200];

  sprintf(header_filename,"/run/shm/s3gethead%s.tmp",curl_handle->id);

  sprintf(resource,"%s/%s",S3_BUCKET,objname);

  curl = curl_handle->curl;

  S3_list_header_fptr=fopen(header_filename,"w+");

  generate_S3_sig("GET", date_string,S3_signature,resource);
  sprintf(date_string_header, "date: %s", date_string);
  sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS, S3_signature);

  printf("%s\n",AWS_auth_string);

  chunk=NULL;

  sprintf(container_string,"%s/%s",S3_BUCKET_URL,objname);
  chunk=curl_slist_append(chunk, "Expect:");
  chunk=curl_slist_append(chunk, date_string_header);
  chunk=curl_slist_append(chunk, AWS_auth_string);


  fseek(fptr,0,SEEK_END);
  objsize=ftell(fptr);
  fseek(fptr,0,SEEK_SET);
  if (objsize < 0)
   {
    fclose(S3_list_header_fptr);
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
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, NULL);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_list_header_fptr);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(S3_list_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);
    return -1;
   }
 
  curl_slist_free_all(chunk);
  ret_val = parse_http_header_retcode(S3_list_header_fptr);
  fclose(S3_list_header_fptr);
  unlink(header_filename);

  return ret_val;
 }
int hcfs_S3_delete_object(char *objname, CURL_HANDLE *curl_handle)
 {
  struct curl_slist *chunk=NULL;
  off_t objsize;
  CURLcode res;
  char container_string[200];
  char delete_command[10];

  FILE *S3_list_header_fptr;
  CURL *curl;
  char header_filename[100];
  int ret_val;

  unsigned char date_string[100];
  char date_string_header[100];
  unsigned char AWS_auth_string[200];
  unsigned char S3_signature[200];
  unsigned char resource[200];


  //printf("Debug S3_delete_object: object is %s\n",objname);

  sprintf(header_filename,"/run/shm/s3deletehead%s.tmp",curl_handle->id);

  sprintf(resource,"%s/%s",S3_BUCKET,objname);

  curl = curl_handle->curl;

  S3_list_header_fptr=fopen(header_filename,"w+");
  strcpy(delete_command,"DELETE");

  generate_S3_sig("DELETE", date_string,S3_signature,resource);
  sprintf(date_string_header, "date: %s", date_string);
  sprintf(AWS_auth_string, "authorization: AWS %s:%s", S3_ACCESS, S3_signature);

  printf("%s\n",AWS_auth_string);

  chunk=NULL;

  sprintf(container_string,"%s/%s",S3_BUCKET_URL,objname);
  chunk=curl_slist_append(chunk, "Expect:");
  chunk=curl_slist_append(chunk, date_string_header);
  chunk=curl_slist_append(chunk, AWS_auth_string);

  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 0L);
  curl_easy_setopt(curl, CURLOPT_PUT, 0L);
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 0L);
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, delete_command);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_file_function);
  curl_easy_setopt(curl, CURLOPT_WRITEHEADER, S3_list_header_fptr);

  curl_easy_setopt(curl,CURLOPT_URL, container_string);
  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   {
    fprintf(stderr, "failed %s\n", curl_easy_strerror(res));
    fclose(S3_list_header_fptr);
    unlink(header_filename);
    curl_slist_free_all(chunk);
    return -1;
   }

  curl_slist_free_all(chunk);
  ret_val = parse_http_header_retcode(S3_list_header_fptr);
  fclose(S3_list_header_fptr);
  unlink(header_filename);

  return ret_val;
 }


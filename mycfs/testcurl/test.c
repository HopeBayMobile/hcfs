#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#define URL_HEADER "X-Storage-Url:"
#define AUTH_HEADER "X-Auth-Token:"

char url_string[200];
char container_string[200];
char auth_string[200];

size_t read_header(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  printf("Start dumping header\n");
  printf("%s",bufptr);
  printf("End dumping header\n");

  if (!strncmp(bufptr,URL_HEADER,strlen(URL_HEADER)))
   {
    printf("Got here\n");
    strcpy(url_string,&bufptr[strlen(URL_HEADER)+1]);
    if (url_string[strlen(url_string)-1]=='\n')
     url_string[strlen(url_string)-2]=0;
    printf("url string %s\n",url_string);
   }
  else if (!strncmp(bufptr,AUTH_HEADER,strlen(AUTH_HEADER)))
   {
    strcpy(auth_string,bufptr);
    if (auth_string[strlen(auth_string)-1]=='\n')
     auth_string[strlen(auth_string)-1]=0;
   }


  return size*nmemb;
 }

size_t read_header1(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  printf("Start dumping header\n");
  printf("%s",bufptr);
  printf("End dumping header\n");
  return size*nmemb;
 }


char tempbuffer[100000];

int main(void)
 {
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();

  if (curl)
   {
    struct curl_slist *chunk=NULL;

    chunk=curl_slist_append(chunk, "X-Storage-User: testgw:testgw2");
    chunk=curl_slist_append(chunk, "X-Storage-Pass: 9UxDxYfZdS6K");

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, read_header);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, tempbuffer);
//    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl,CURLOPT_URL, "https://swift.delcloudia.com:8080/auth/v1.0");
//    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
    res = curl_easy_perform(curl);
    if (res!=CURLE_OK)
     fprintf(stderr, "failed %s\n", curl_easy_strerror(res));

    curl_slist_free_all(chunk);

    chunk=NULL;

    printf("\n");
    printf("url string is %s\n\n\n",url_string);
    sprintf(container_string,"%s/testgw2_private_container",url_string);
    printf("container string %s\n",container_string);
    printf("auth string %s\n",auth_string);
//    chunk=curl_slist_append(chunk, container_string);
    chunk=curl_slist_append(chunk, auth_string);

    printf("container string is %s\n\n\n",container_string);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, read_header1);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, tempbuffer);
//    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl,CURLOPT_URL, container_string);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
    res = curl_easy_perform(curl);
    if (res!=CURLE_OK)
     fprintf(stderr, "failed %s\n", curl_easy_strerror(res));


    curl_easy_cleanup(curl);
    curl_slist_free_all(chunk);
   }
  return 0;
 }

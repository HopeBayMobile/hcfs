#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

size_t read_header(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  printf("Start dumping header\n");
  printf("%s\n",bufptr);
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

    chunk=curl_slist_append(chunk, "X-Storage-User: demo:demo");
    chunk=curl_slist_append(chunk, "X-Storage-Pass: cRpM9k3JtR3N");

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, read_header);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, tempbuffer);
//    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl,CURLOPT_URL, "https://172.16.78.238:8080/auth/v1.0");
//    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
    res = curl_easy_perform(curl);
    if (res!=CURLE_OK)
     fprintf(stderr, "failed %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    curl_slist_free_all(chunk);
   }
  return 0;
 }

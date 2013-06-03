#include "mycurl.h"

size_t read_header_dummy(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  return size*nmemb;
 }

size_t write_function(void *bufptr, size_t size, size_t nmemb, void *tempbuffer)
 {
  FILE *fptr;

  fptr=fopen("tempstore","a+");

  fwrite(bufptr,size,nmemb,fptr);

  fclose(fptr);
  return size*nmemb;
 }


char tempbuffer[1000];
char tempwritebuffer[100];

int main(void)
 {
  curl = curl_easy_init();

  if (curl)
   {
    struct curl_slist *chunk=NULL;

    swift_get_auth_info("testgw:testgw2", "9UxDxYfZdS6K", "swift.delcloudia.com:8080");

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
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, read_header_dummy);
    curl_easy_setopt(curl, CURLOPT_WRITEHEADER, tempbuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, tempwritebuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_function);
//    curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
    curl_easy_setopt(curl,CURLOPT_URL, container_string);
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

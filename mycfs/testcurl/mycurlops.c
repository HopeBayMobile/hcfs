#include "mycurl.h"

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
void swift_get_auth_info(char *swift_user,char *swift_pass, char *swift_url)
 {
  char userstring[1000];
  char passstring[1000];
  char urlstring[1000];
  struct curl_slist *chunk=NULL;

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

  curl_easy_setopt(curl,CURLOPT_HTTPHEADER, chunk);
  res = curl_easy_perform(curl);
  if (res!=CURLE_OK)
   fprintf(stderr, "failed %s\n", curl_easy_strerror(res));

  curl_slist_free_all(chunk);

  return;
 }

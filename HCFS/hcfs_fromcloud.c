#include "params.h"
#include "hcfscurl.h"
#include "fuseop.h"

void fetch_from_cloud(FILE *fptr, ino_t this_inode, long block_no)
 {
  char objname[1000];
  int status;
  int which_curl_handle;
  char idname[256];

  sprintf(objname,"data_%ld_%ld",this_inode,block_no);
  while(1==1)
   {
    sem_wait(&download_curl_sem);
    fseek(fptr,0,SEEK_SET);
    ftruncate(fileno(fptr),0);
    for(which_curl_handle=0;which_curl_handle<MAX_DOWNLOAD_CURL_HANDLE;which_curl_handle++)
     {
      if (curl_handle_mask[which_curl_handle] == FALSE)
       {
        curl_handle_mask[which_curl_handle] = TRUE;
        break;
       }
     }
    printf("Debug: downloading using curl handle %d\n",which_curl_handle);
    sprintf(idname,"download_thread_%d",which_curl_handle);
    strcpy(download_curl_handles[which_curl_handle].id,idname);
    status=hcfs_swift_get_object(fptr,objname,&(download_curl_handles[which_curl_handle]));
    curl_handle_mask[which_curl_handle] = FALSE;
    sem_post(&download_curl_sem);
    if ((status< 200) || (status > 299))
     {
      while ((status< 200) || (status > 299))
        status = hcfs_swift_reauth();
      printf("Reauth\n");
     }
    else
     break;
   }

  fflush(fptr);
  return;
 }


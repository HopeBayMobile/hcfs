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
    sem_wait(&download_curl_control_sem);
    for(which_curl_handle=0;which_curl_handle<MAX_DOWNLOAD_CURL_HANDLE;which_curl_handle++)
     {
      if (curl_handle_mask[which_curl_handle] == FALSE)
       {
        curl_handle_mask[which_curl_handle] = TRUE;
        break;
       }
     }
    sem_post(&download_curl_control_sem);
    printf("Debug: downloading using curl handle %d\n",which_curl_handle);
    sprintf(idname,"download_thread_%d",which_curl_handle);
    strcpy(download_curl_handles[which_curl_handle].id,idname);
    status=hcfs_swift_get_object(fptr,objname,&(download_curl_handles[which_curl_handle]));

    if ((status< 200) || (status > 299))
     {
      while ((status< 200) || (status > 299))
        status = hcfs_swift_reauth(&(download_curl_handles[which_curl_handle]));
      printf("Reauth\n");

      sem_wait(&download_curl_control_sem);
      curl_handle_mask[which_curl_handle] = FALSE;
      sem_post(&download_curl_sem);
      sem_post(&download_curl_control_sem);
     }
    else
     {
      sem_wait(&download_curl_control_sem);
      curl_handle_mask[which_curl_handle] = FALSE;
      sem_post(&download_curl_sem);
      sem_post(&download_curl_control_sem);
      break;
     }
   }

  fflush(fptr);
  return;
 }

void prefetch_block(PREFETCH_STRUCT_TYPE *ptr)
 {
  FILE *metafptr;
  FILE *blockfptr;
  char thisblockpath[400];
  char thismetapath[400];
  BLOCK_ENTRY_PAGE temppage;
  int entry_index;
  struct stat tempstat;

  entry_index = ptr->entry_index;
  /*Download from backend */
  fetch_meta_path(thismetapath,ptr->this_inode);
  fetch_block_path(thisblockpath,ptr->this_inode,ptr->block_no);

  metafptr = fopen(thismetapath,"r+");
  if (metafptr == NULL)
   {
    free(ptr);
    return;
   }
  setbuf(metafptr,NULL);

  blockfptr = fopen(thisblockpath,"a+");
  fclose(blockfptr);
  
  blockfptr = fopen(thisblockpath,"r+");
  setbuf(blockfptr,NULL);
  flock(fileno(blockfptr),LOCK_EX);

  flock(fileno(metafptr),LOCK_EX);
  fseek(metafptr, ptr->page_start_fpos,SEEK_SET);
  fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,metafptr);

  if (((temppage).block_entries[entry_index].status == ST_CLOUD) ||
      ((temppage).block_entries[entry_index].status == ST_CtoL))
   {
    if ((temppage).block_entries[entry_index].status == ST_CLOUD)
     {
      (temppage).block_entries[entry_index].status = ST_CtoL;
      fseek(metafptr, ptr->page_start_fpos,SEEK_SET);
      fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
      fflush(metafptr);
     }
    flock(fileno(metafptr),LOCK_UN);
    fetch_from_cloud(blockfptr,ptr->this_inode, ptr->block_no);
    /*Do not process cache update and stored_where change if block is actually deleted by other ops such as truncate*/
    flock(fileno(metafptr),LOCK_EX);
    fseek(metafptr, ptr->page_start_fpos,SEEK_SET);
    fread(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
    if (stat(thisblockpath,&tempstat)==0)
     {
      (temppage).block_entries[entry_index].status = ST_BOTH;
      fseek(metafptr, ptr->page_start_fpos,SEEK_SET);
      fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
      fflush(metafptr);

      sem_wait(&(hcfs_system->access_sem));
      hcfs_system->systemdata.cache_size += tempstat.st_size;
      hcfs_system->systemdata.cache_blocks++;
      sync_hcfs_system_data(FALSE);
      sem_post(&(hcfs_system->access_sem));           
     }
   }
  flock(fileno(blockfptr),LOCK_UN);
  fclose(blockfptr);
  flock(fileno(metafptr),LOCK_UN);
  fclose(metafptr);
  free(ptr);
  return;
 }

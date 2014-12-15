/*
TODO: Will need to check mod time of meta file and not upload meta for every block status change.
TODO: Check if meta objects will be deleted with the deletion of files/dirs
TODO: error handling for HTTP exceptions
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <pthread.h>

#include "hcfs_tocloud.h"
#include "hcfs_clouddelete.h"
#include "params.h"
#include "global.h"
#include "hcfscurl.h"
#include "super_block.h"
#include "fuseop.h"

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

void collect_finished_sync_threads(void *ptr)
 {
  int count;
  int ret_val;
  struct timespec time_to_sleep;

  time_to_sleep.tv_sec = 0;
  time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/
  while(TRUE)  /*TODO: Perhaps need to change this flag to allow terminating at shutdown*/
   {
    sem_wait(&(sync_thread_control.sync_op_sem));

    if (sync_thread_control.total_active_sync_threads <=0)
     {
      sem_post(&(sync_thread_control.sync_op_sem));
      nanosleep(&time_to_sleep,NULL);
      continue;
     }
    for(count=0;count<MAX_SYNC_CONCURRENCY;count++)
     {
      if ((sync_thread_control.sync_threads_in_use[count]!=0) && (sync_thread_control.sync_threads_created[count] == TRUE))
       {
        ret_val = pthread_tryjoin_np(sync_thread_control.inode_sync_thread[count],NULL);
        if (ret_val == 0)
         {
          sync_thread_control.sync_threads_in_use[count]=0;
          sync_thread_control.sync_threads_created[count] == FALSE;
          sync_thread_control.total_active_sync_threads --;
          sem_post(&(sync_thread_control.sync_queue_sem));
         }
       }
     }
    sem_post(&(sync_thread_control.sync_op_sem));
    nanosleep(&time_to_sleep,NULL);
    continue;
   }
  return;
 }

void collect_finished_upload_threads(void *ptr)
 {
  int count,count2;
  int which_curl;
  int ret_val;
  FILE *metafptr;
  char thismetapath[METAPATHLEN];
  char blockpath[400];
  ino_t this_inode;
  off_t page_filepos;
  long long page_entry_index;
  long long blockno;
  BLOCK_ENTRY_PAGE temppage;
  struct timespec time_to_sleep;
  char is_delete;

  time_to_sleep.tv_sec = 0;
  time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/


  while(TRUE)  /*TODO: Perhaps need to change this flag to allow terminating at shutdown*/
   {
    sem_wait(&(upload_thread_control.upload_op_sem));

    if (upload_thread_control.total_active_upload_threads <=0)
     {
      sem_post(&(upload_thread_control.upload_op_sem));
      nanosleep(&time_to_sleep,NULL);
      continue;
     }
    for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
     {
      if (((upload_thread_control.upload_threads_in_use[count]!=0) && (upload_thread_control.upload_threads[count].is_block == TRUE)) && (upload_thread_control.upload_threads_created[count] == TRUE))
       {
        ret_val = pthread_tryjoin_np(upload_thread_control.upload_threads_no[count],NULL);
        if (ret_val == 0)
         {
          this_inode = upload_thread_control.upload_threads[count].inode;
          is_delete = upload_thread_control.upload_threads[count].is_delete;
          page_filepos = upload_thread_control.upload_threads[count].page_filepos;
          page_entry_index = upload_thread_control.upload_threads[count].page_entry_index;
          blockno = upload_thread_control.upload_threads[count].blockno;
          fetch_meta_path(thismetapath,this_inode);

          if (!access(thismetapath,F_OK)) /*Perhaps the file is deleted already*/
           {
            metafptr = fopen(thismetapath,"r+");
            if (metafptr!=NULL)
             {
              setbuf(metafptr,NULL);
              flock(fileno(metafptr),LOCK_EX);
              if (!access(thismetapath,F_OK)) /*Perhaps the file is deleted already*/
               {
                fseek(metafptr,page_filepos,SEEK_SET);
                fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
                if ((temppage.block_entries[page_entry_index].status==ST_LtoC) && (is_delete == FALSE))
                 {
                  temppage.block_entries[page_entry_index].status=ST_BOTH;
                  fetch_block_path(blockpath,this_inode,blockno);
                  setxattr(blockpath,"user.dirty","F",1,0);
                  fseek(metafptr,page_filepos,SEEK_SET);
                  fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
                 }
                else
                 {
                  if ((temppage.block_entries[page_entry_index].status==ST_TODELETE) && (is_delete == TRUE))
                   {
                    temppage.block_entries[page_entry_index].status=ST_NONE;
                    fseek(metafptr,page_filepos,SEEK_SET);
                    fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
                   }
                 }
                /*TODO: Check if status is ST_NONE. If so, the block is removed due to truncating. Need to schedule block for deletion due to truncating*/
               }
              flock(fileno(metafptr),LOCK_UN);
              fclose(metafptr);
             }
           }
          /*TODO: If metafile is deleted already, schedule the block to be deleted.*/
          /*TODO: This must be before the usage flag is cleared*/

          if (access(thismetapath,F_OK)==-1)  /*If file is deleted*/
           {
            sem_wait(&(delete_thread_control.delete_queue_sem));
            sem_wait(&(delete_thread_control.delete_op_sem));
            which_curl = -1;
            for(count2=0;count2<MAX_DELETE_CONCURRENCY;count2++)
             {
              if (delete_thread_control.delete_threads_in_use[count2] == FALSE)
               {
                delete_thread_control.delete_threads_in_use[count2] = TRUE;
                delete_thread_control.delete_threads_created[count2] = FALSE;
                delete_thread_control.delete_threads[count2].is_block = TRUE;
                delete_thread_control.delete_threads[count2].inode = this_inode;
                delete_thread_control.delete_threads[count2].blockno = blockno;
                delete_thread_control.delete_threads[count2].which_curl = count2;

                delete_thread_control.total_active_delete_threads++;
                which_curl = count2;
                break;
               }
             }
            sem_post(&(delete_thread_control.delete_op_sem));
            pthread_create(&(delete_thread_control.delete_threads_no[which_curl]),NULL, (void *)&con_object_dsync,(void *)&(delete_thread_control.delete_threads[which_curl]));
            delete_thread_control.delete_threads_created[which_curl]=TRUE;
           }



          upload_thread_control.upload_threads_in_use[count]=FALSE;
          upload_thread_control.upload_threads_created[count]=FALSE;
          upload_thread_control.total_active_upload_threads --;
          sem_post(&(upload_thread_control.upload_queue_sem));
         }
       }
     }
    sem_post(&(upload_thread_control.upload_op_sem));
    nanosleep(&time_to_sleep,NULL);
    continue;
   }
  return;
 }

void init_sync_control()
 {
  memset(&sync_thread_control,0,sizeof(SYNC_THREAD_CONTROL));
  sem_init(&(sync_thread_control.sync_op_sem),0,1);
  sem_init(&(sync_thread_control.sync_queue_sem),0,MAX_SYNC_CONCURRENCY);
  memset(&(sync_thread_control.sync_threads_in_use),0,sizeof(ino_t)*MAX_SYNC_CONCURRENCY);
  memset(&(sync_thread_control.sync_threads_created),0,sizeof(char)*MAX_SYNC_CONCURRENCY);
  sync_thread_control.total_active_sync_threads = 0;

  pthread_create(&(sync_thread_control.sync_handler_thread),NULL,(void *)&collect_finished_sync_threads, NULL);

  return;
 }

void init_upload_control()
 {
  int count,ret_val;

  memset(&upload_thread_control,0,sizeof(UPLOAD_THREAD_CONTROL));
  memset(&upload_curl_handles,0,sizeof(CURL_HANDLE)*MAX_UPLOAD_CONCURRENCY);
  for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
   {
    ret_val = hcfs_init_backend(&(upload_curl_handles[count]));

   }

  sem_init(&(upload_thread_control.upload_op_sem),0,1);
  sem_init(&(upload_thread_control.upload_queue_sem),0,MAX_UPLOAD_CONCURRENCY);
  memset(&(upload_thread_control.upload_threads_in_use),0,sizeof(char)*MAX_UPLOAD_CONCURRENCY);
  memset(&(upload_thread_control.upload_threads_created),0,sizeof(char)*MAX_UPLOAD_CONCURRENCY);
  upload_thread_control.total_active_upload_threads = 0;

  pthread_create(&(upload_thread_control.upload_handler_thread),NULL,(void *)&collect_finished_upload_threads, NULL);

  return;
 }


void sync_single_inode(SYNC_THREAD_TYPE *ptr)
 {
  char thismetapath[METAPATHLEN];
  ino_t this_inode;
  FILE *metafptr;
  struct stat tempfilestat;
  FILE_META_TYPE tempfilemeta;
  BLOCK_ENTRY_PAGE temppage;
  int which_curl;
  long long page_pos,block_no, current_entry_index;
  long long total_blocks,total_pages;
  long long count, block_count;
  unsigned char block_status;
  char upload_done;
  int ret_val;
  struct timespec time_to_sleep;

  time_to_sleep.tv_sec = 0;
  time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

  this_inode = ptr->inode;

  fetch_meta_path(thismetapath,this_inode);

  metafptr=fopen(thismetapath,"r+");
  if (metafptr == NULL)
   {
    super_block_update_transit(ptr->inode,FALSE);
    return;
   }

  setbuf(metafptr,NULL);

  if ((ptr->this_mode) & S_IFREG)
   {
    flock(fileno(metafptr),LOCK_EX);
    fread(&tempfilestat,sizeof(struct stat),1,metafptr);
    fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,metafptr);
    page_pos=tempfilemeta.next_block_page;
    current_entry_index = 0;
    if (tempfilestat.st_size == 0)
     total_blocks = 0;
    else
     total_blocks = ((tempfilestat.st_size - 1) / MAX_BLOCK_SIZE) + 1;

    if (total_blocks ==0)
     total_pages = 0;
    else
     total_pages = ((total_blocks - 1) / MAX_BLOCK_ENTRIES_PER_PAGE) + 1;

    flock(fileno(metafptr),LOCK_UN);

    for(block_count=0;page_pos!=0;block_count++)
     {
      flock(fileno(metafptr),LOCK_EX);

      if (access(thismetapath,F_OK)<0) /*Perhaps the file is deleted already*/
       {
        flock(fileno(metafptr),LOCK_UN);
        break;
       }

      if (current_entry_index >= MAX_BLOCK_ENTRIES_PER_PAGE)
       {
        page_pos = temppage.next_page;
        current_entry_index = 0;
        if (page_pos == 0)
         {
          flock(fileno(metafptr),LOCK_UN);
          break;
         }
       }

/*TODO: error handling here if cannot read correctly*/

      fseek(metafptr,page_pos,SEEK_SET);
      if (ftell(metafptr)!=page_pos)
       {
        flock(fileno(metafptr),LOCK_UN);
        break;
       }

      ret_val = fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
      if (ret_val < 1)
       {
        flock(fileno(metafptr),LOCK_UN);
        break;
       }


      block_status = temppage.block_entries[current_entry_index].status;

      if (((block_status == ST_LDISK) || (block_status == ST_LtoC)) && (block_count < total_blocks))
       {
        if (block_status == ST_LDISK)
         {
          temppage.block_entries[current_entry_index].status = ST_LtoC;
          fseek(metafptr,page_pos,SEEK_SET);
          fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
         }      
        flock(fileno(metafptr),LOCK_UN);
        sem_wait(&(upload_thread_control.upload_queue_sem));
        sem_wait(&(upload_thread_control.upload_op_sem));
        which_curl = -1;
        for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
         {
          if (upload_thread_control.upload_threads_in_use[count] == FALSE)
           {
            upload_thread_control.upload_threads_in_use[count] = TRUE;
            upload_thread_control.upload_threads_created[count] = FALSE;
            upload_thread_control.upload_threads[count].is_block = TRUE;
            upload_thread_control.upload_threads[count].is_delete = FALSE;
            upload_thread_control.upload_threads[count].inode = ptr->inode;
            upload_thread_control.upload_threads[count].blockno = block_count;
            upload_thread_control.upload_threads[count].page_filepos = page_pos;
            upload_thread_control.upload_threads[count].page_entry_index = current_entry_index;
            upload_thread_control.upload_threads[count].which_curl = count;

            upload_thread_control.total_active_upload_threads++;
            which_curl = count;
            break;
           }
         }
        sem_post(&(upload_thread_control.upload_op_sem));
        dispatch_upload_block(which_curl); /*Maybe should also first copy block out first*/
       }
      else
       {
        if (block_status == ST_TODELETE)
         {
          flock(fileno(metafptr),LOCK_UN);
          sem_wait(&(upload_thread_control.upload_queue_sem));
          sem_wait(&(upload_thread_control.upload_op_sem));
          which_curl = -1;
          for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
           {
            if (upload_thread_control.upload_threads_in_use[count] == FALSE)
             {
              upload_thread_control.upload_threads_in_use[count] = TRUE;
              upload_thread_control.upload_threads_created[count] = FALSE;
              upload_thread_control.upload_threads[count].is_block = TRUE;
              upload_thread_control.upload_threads[count].is_delete = TRUE;
              upload_thread_control.upload_threads[count].inode = ptr->inode;
              upload_thread_control.upload_threads[count].blockno = block_count;
              upload_thread_control.upload_threads[count].page_filepos = page_pos;
              upload_thread_control.upload_threads[count].page_entry_index = current_entry_index;
              upload_thread_control.upload_threads[count].which_curl = count;

              upload_thread_control.total_active_upload_threads++;
              which_curl = count;
              break;
             }
           }
          sem_post(&(upload_thread_control.upload_op_sem));
          dispatch_delete_block(which_curl); /*Maybe should also first copy block out first*/
         }
        else
         flock(fileno(metafptr),LOCK_UN);
       }

      current_entry_index++;
     }
/* Block sync should be done here. Check if all upload threads for this inode has returned before starting meta sync*/

    upload_done = FALSE;
    while(upload_done == FALSE)
     {
      nanosleep(&time_to_sleep,NULL);
      upload_done = TRUE;
      sem_wait(&(upload_thread_control.upload_op_sem));
      for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
       {
        if ((upload_thread_control.upload_threads_in_use[count] == TRUE) && (upload_thread_control.upload_threads[count].inode == ptr->inode))
         {
          upload_done = FALSE;
          break;
         }
       }
      sem_post(&(upload_thread_control.upload_op_sem));
     }
   }

/*Check if metafile still exists. If not, forget the meta upload*/
  if (access(thismetapath,F_OK)<0)
   return;

  sem_wait(&(upload_thread_control.upload_queue_sem));
  sem_wait(&(upload_thread_control.upload_op_sem));
  which_curl = -1;
  for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
   {
    if (upload_thread_control.upload_threads_in_use[count] == FALSE)
     {
      upload_thread_control.upload_threads_in_use[count] = TRUE;
      upload_thread_control.upload_threads_created[count] = FALSE;
      upload_thread_control.upload_threads[count].is_block = FALSE;
      upload_thread_control.upload_threads[count].is_delete = FALSE;
      upload_thread_control.upload_threads[count].inode = ptr->inode;
      upload_thread_control.upload_threads[count].which_curl = count;
      upload_thread_control.total_active_upload_threads++;
      which_curl = count;
      break;
     }
   }
  sem_post(&(upload_thread_control.upload_op_sem));

  flock(fileno(metafptr),LOCK_EX);
  /*Check if metafile still exists. If not, forget the meta upload*/
  if (!access(thismetapath,F_OK))
   {
    schedule_sync_meta(metafptr,which_curl);
    flock(fileno(metafptr),LOCK_UN);
    fclose(metafptr);

    pthread_join(upload_thread_control.upload_threads_no[which_curl],NULL);
  /*TODO: Need to check if metafile still exists. If not, schedule the deletion of meta*/

    sem_wait(&(upload_thread_control.upload_op_sem));
    upload_thread_control.upload_threads_in_use[which_curl] = FALSE;
    upload_thread_control.upload_threads_created[which_curl] = FALSE;
    upload_thread_control.total_active_upload_threads--;
    sem_post(&(upload_thread_control.upload_op_sem));
    sem_post(&(upload_thread_control.upload_queue_sem));

    super_block_update_transit(ptr->inode,FALSE);

   }
  else
   {
    flock(fileno(metafptr),LOCK_UN);
    fclose(metafptr);

    sem_wait(&(upload_thread_control.upload_op_sem));
    upload_thread_control.upload_threads_in_use[which_curl] = FALSE;
    upload_thread_control.upload_threads_created[which_curl] = FALSE;
    upload_thread_control.total_active_upload_threads--;
    sem_post(&(upload_thread_control.upload_op_sem));
    sem_post(&(upload_thread_control.upload_queue_sem));
   }

  return;
 }


void do_block_sync(ino_t this_inode, long long block_no, CURL_HANDLE *curl_handle, char *filename)
 {
  char objname[1000];
  FILE *fptr;
  int ret_val;
 
  sprintf(objname,"data_%lld_%lld",this_inode,block_no);
  printf("Debug datasync: objname %s, inode %lld, block %lld\n",objname,this_inode,block_no);
  sprintf(curl_handle->id,"upload_blk_%lld_%lld",this_inode,block_no);
  fptr=fopen(filename,"r");
  ret_val = hcfs_put_object(fptr,objname, curl_handle);
  fclose(fptr);
  return;
 }

void do_block_delete(ino_t this_inode, long long block_no, CURL_HANDLE *curl_handle)
 {
  char objname[1000];
  int ret_val;
 
  sprintf(objname,"data_%lld_%lld",this_inode,block_no);
  printf("Debug delete object: objname %s, inode %lld, block %lld\n",objname,this_inode,block_no);
  sprintf(curl_handle->id,"delete_blk_%lld_%lld",this_inode,block_no);
  ret_val = hcfs_delete_object(objname, curl_handle);
  return;
 }

void do_meta_sync(ino_t this_inode, CURL_HANDLE *curl_handle, char *filename)
 {
  char objname[1000];
  int ret_val;
  FILE *fptr;

  sprintf(objname,"meta_%lld",this_inode);
  printf("Debug datasync: objname %s, inode %lld\n",objname,this_inode);
  sprintf(curl_handle->id,"upload_meta_%lld",this_inode);
  fptr=fopen(filename,"r");
  ret_val = hcfs_put_object(fptr,objname, curl_handle);
  fclose(fptr);
  return;
 }

void con_object_sync(UPLOAD_THREAD_TYPE *upload_thread_ptr)
 {
  int which_curl;
  which_curl = upload_thread_ptr->which_curl;
  if (upload_thread_ptr->is_block == TRUE)
   do_block_sync(upload_thread_ptr->inode, upload_thread_ptr->blockno, &(upload_curl_handles[which_curl]), upload_thread_ptr->tempfilename);
  else
   do_meta_sync(upload_thread_ptr->inode, &(upload_curl_handles[which_curl]), upload_thread_ptr->tempfilename);

  unlink(upload_thread_ptr->tempfilename);
  return;
 }

void delete_object_sync(UPLOAD_THREAD_TYPE *upload_thread_ptr)
 {
  int which_curl;
  which_curl = upload_thread_ptr->which_curl;
  if (upload_thread_ptr->is_block == TRUE)
   do_block_delete(upload_thread_ptr->inode, upload_thread_ptr->blockno, &(upload_curl_handles[which_curl]));

  return;
 }

void schedule_sync_meta(FILE *metafptr,int which_curl)
 {
  char tempfilename[400];
  char filebuf[4100];
  int read_size;
  int count;
  FILE *fptr;

  sprintf(tempfilename,"/dev/shm/hcfs_sync_meta_%lld.tmp", upload_thread_control.upload_threads[which_curl].inode);

  count = 0;
  while(TRUE)
   {  
    if (!access(tempfilename,F_OK))
     {
      count++;
      sprintf(tempfilename,"/dev/shm/hcfs_sync_meta_%lld.%d", upload_thread_control.upload_threads[which_curl].inode,count);
     }
    else
     break;
   }

  fptr = fopen(tempfilename,"w");
  fseek(metafptr,0,SEEK_SET);
  while(!feof(metafptr))
   {
    read_size = fread(filebuf,1,4096,metafptr);
    if (read_size > 0)
     {
      fwrite(filebuf,1,read_size,fptr);
     }
    else
     break;
   }
  fclose(fptr);

  strcpy(upload_thread_control.upload_threads[which_curl].tempfilename,tempfilename);
  pthread_create(&(upload_thread_control.upload_threads_no[which_curl]),NULL, (void *)&con_object_sync,(void *)&(upload_thread_control.upload_threads[which_curl]));
  upload_thread_control.upload_threads_created[which_curl] = TRUE;

  return;
 }

void dispatch_upload_block(int which_curl)
 {
  char tempfilename[400];
  char thisblockpath[400];
  char filebuf[4100];
  int read_size;
  int count;
  FILE *fptr,*blockfptr;

  sprintf(tempfilename,"/dev/shm/hcfs_sync_block_%lld_%lld.tmp", upload_thread_control.upload_threads[which_curl].inode,upload_thread_control.upload_threads[which_curl].blockno);

  count = 0;
  while(TRUE)
   {  
    if (!access(tempfilename,F_OK))
     {
      count++;
      sprintf(tempfilename,"/dev/shm/hcfs_sync_meta_%lld_%lld.%d", upload_thread_control.upload_threads[which_curl].inode,upload_thread_control.upload_threads[which_curl].blockno, count);
     }
    else
     break;
   }

  fetch_block_path(thisblockpath, upload_thread_control.upload_threads[which_curl].inode,upload_thread_control.upload_threads[which_curl].blockno);

  blockfptr = fopen(thisblockpath,"r");
  if (blockfptr !=NULL)
   {
    flock(fileno(blockfptr),LOCK_EX);
    fptr = fopen(tempfilename,"w");
    while(!feof(blockfptr))
     {
      read_size = fread(filebuf,1,4096,blockfptr);
      if (read_size > 0)
       {
        fwrite(filebuf,1,read_size,fptr);
       }
      else
       break;
     }
    flock(fileno(blockfptr),LOCK_UN);
    fclose(blockfptr);
    fclose(fptr);

    strcpy(upload_thread_control.upload_threads[which_curl].tempfilename,tempfilename);
    pthread_create(&(upload_thread_control.upload_threads_no[which_curl]),NULL, (void *)&con_object_sync,(void *)&(upload_thread_control.upload_threads[which_curl]));
    upload_thread_control.upload_threads_created[which_curl]=TRUE;
   }
  else
   {  /*Block is gone. Undo changes*/
    sem_wait(&(upload_thread_control.upload_op_sem));
    upload_thread_control.upload_threads_in_use[which_curl] = FALSE;
    upload_thread_control.upload_threads_created[which_curl] = FALSE;
    upload_thread_control.total_active_upload_threads--;
    sem_post(&(upload_thread_control.upload_op_sem));
    sem_post(&(upload_thread_control.upload_queue_sem));
   }

  return;
 }
void dispatch_delete_block(int which_curl)
 {
  char tempfilename[400];
  char thisblockpath[400];
  char filebuf[4100];
  int read_size;
  int count;
  FILE *fptr,*blockfptr;

  pthread_create(&(upload_thread_control.upload_threads_no[which_curl]),NULL, (void *)&delete_object_sync,(void *)&(upload_thread_control.upload_threads[which_curl]));
  upload_thread_control.upload_threads_created[which_curl]=TRUE;

  return;
 }

void upload_loop()
 {
  ino_t inode_to_sync, inode_to_check;
  SYNC_THREAD_TYPE sync_threads[MAX_SYNC_CONCURRENCY];
  SUPER_BLOCK_ENTRY tempentry;
  int count,sleep_count;
  char in_sync;
  int ret_val;
  char do_something;

  init_upload_control();
  init_sync_control();

  printf("Start upload loop\n");

  while(TRUE)
   {
    for (sleep_count=0;sleep_count<30;sleep_count++)
     {
      if (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT) /*Sleep for a while if we are not really in a hurry*/
      sleep(1);
      else
      break;
     }

    inode_to_check = 0;
    do_something = FALSE;
    while(TRUE)
     {
      sem_wait(&(sync_thread_control.sync_queue_sem));
      super_block_exclusive_locking();
      if (inode_to_check == 0)
       inode_to_check = sys_super_block->head.first_dirty_inode;
      inode_to_sync = 0;
      if (inode_to_check !=0)
       {
        inode_to_sync = inode_to_check;

        ret_val = read_super_block_entry(inode_to_sync,&tempentry);

        if ((ret_val < 0) || (tempentry.status!=IS_DIRTY))
         {
          inode_to_sync = 0;
          inode_to_check = 0;
         }
        else
         {
          if (tempentry.in_transit == TRUE)
           {
            inode_to_check = tempentry.util_ll_next;
           }
          else
           {
            tempentry.in_transit = TRUE;
            tempentry.mod_after_in_transit = FALSE;
            inode_to_check = tempentry.util_ll_next;
            write_super_block_entry(inode_to_sync, &tempentry);
           }
         }
       }
      super_block_exclusive_release();
      
      if (inode_to_sync!=0)
       {
        sem_wait(&(sync_thread_control.sync_op_sem));
        /*First check if this inode is actually being synced now*/
        in_sync = FALSE;
        for(count=0;count<MAX_SYNC_CONCURRENCY;count++)
         {
          if (sync_thread_control.sync_threads_in_use[count]==inode_to_sync)
           {
            in_sync = TRUE;
            break;
           }
         }

        if (in_sync == FALSE)
         {
          for(count=0;count<MAX_SYNC_CONCURRENCY;count++)
           {
            if (sync_thread_control.sync_threads_in_use[count]==0)
             {
              sync_thread_control.sync_threads_in_use[count]=inode_to_sync;
              sync_thread_control.sync_threads_created[count]=FALSE;
              sync_threads[count].inode = inode_to_sync;
              sync_threads[count].this_mode = tempentry.inode_stat.st_mode;
              pthread_create(&(sync_thread_control.inode_sync_thread[count]),NULL, (void *)&sync_single_inode,(void *)&(sync_threads[count]));
              sync_thread_control.sync_threads_created[count]=TRUE;
              sync_thread_control.total_active_sync_threads++;
              break;
             }
           }
          do_something = TRUE;
          sem_post(&(sync_thread_control.sync_op_sem));
         }
        else  /*If already syncing to cloud*/
         {
          sem_post(&(sync_thread_control.sync_op_sem));
          sem_post(&(sync_thread_control.sync_queue_sem));
         }        
       }
      else
       {      
        sem_post(&(sync_thread_control.sync_queue_sem));
       }
      if (inode_to_check == 0)
       {
        if (do_something == FALSE)
         sleep(5);
        break;
       }
     }
   }
  return;
 }


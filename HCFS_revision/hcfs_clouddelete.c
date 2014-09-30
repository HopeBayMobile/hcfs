/*TODO: Flow for delete loop will be similar to that of upload loop. Will first scan the to_be_deleted linked list in super inode and then check if the meta is in the to_delete temp dir. Open it and and start to delete the blocks first, then delete the meta last.*/

/*TODO: before actually moving the inode from to_be_deleted to deleted, must first check the upload threads and sync threads to find out if there are any pending uploads. It must wait until those are cleared. It must then wait for any additional pending meta or block deletion for this inode to finish.*/

/*TODO: Current issue: If delete file when objects of that file is still being uploaded, objects for that file won't be deleted completely.*/

/*TODO: Need to run super inode reclaim after moving inode from to_delete to deleted*/


#include "hcfs_clouddelete.h"
#include "hcfs_tocloud.h"
#include "params.h"
#include "hcfscurl.h"
#include "super_inode.h"
#include "fuseop.h"
#include "global.h"
#include <time.h>

CURL_HANDLE delete_curl_handles[MAX_DELETE_CONCURRENCY];

void collect_finished_dsync_threads(void *ptr)
 {
  int count;
  int ret_val;
  struct timespec time_to_sleep;

  time_to_sleep.tv_sec = 0;
  time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/
  while(TRUE)  /*TODO: Perhaps need to change this flag to allow terminating at shutdown*/
   {
    sem_wait(&(dsync_thread_control.dsync_op_sem));

    if (dsync_thread_control.total_active_dsync_threads <=0)
     {
      sem_post(&(dsync_thread_control.dsync_op_sem));
      nanosleep(&time_to_sleep,NULL);
      continue;
     }
    for(count=0;count<MAX_DSYNC_CONCURRENCY;count++)
     {
      if ((dsync_thread_control.dsync_threads_in_use[count]!=0) && (dsync_thread_control.dsync_threads_created[count] == TRUE))
       {
        ret_val = pthread_tryjoin_np(dsync_thread_control.inode_dsync_thread[count],NULL);
        if (ret_val == 0)
         {
          dsync_thread_control.dsync_threads_in_use[count]=0;
          dsync_thread_control.dsync_threads_created[count] == FALSE;
          dsync_thread_control.total_active_dsync_threads --;
          sem_post(&(dsync_thread_control.dsync_queue_sem));
         }
       }
     }
    sem_post(&(dsync_thread_control.dsync_op_sem));
    nanosleep(&time_to_sleep,NULL);
    continue;
   }
  return;
 }

void collect_finished_delete_threads(void *ptr)
 {
  int count;
  int ret_val;
  struct timespec time_to_sleep;

  time_to_sleep.tv_sec = 0;
  time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/


  while(TRUE)  /*TODO: Perhaps need to change this flag to allow terminating at shutdown*/
   {
    sem_wait(&(delete_thread_control.delete_op_sem));

    if (delete_thread_control.total_active_delete_threads <=0)
     {
      sem_post(&(delete_thread_control.delete_op_sem));
      nanosleep(&time_to_sleep,NULL);
      continue;
     }
    for(count=0;count<MAX_DELETE_CONCURRENCY;count++)
     {
      if (((delete_thread_control.delete_threads_in_use[count]!=0) && (delete_thread_control.delete_threads[count].is_block == TRUE)) && (delete_thread_control.delete_threads_created[count] == TRUE))
       {
        ret_val = pthread_tryjoin_np(delete_thread_control.delete_threads_no[count],NULL);
        if (ret_val == 0)
         {
          delete_thread_control.delete_threads_in_use[count]=FALSE;
          delete_thread_control.delete_threads_created[count]=FALSE;
          delete_thread_control.total_active_delete_threads --;
          sem_post(&(delete_thread_control.delete_queue_sem));
         }
       }
     }
    sem_post(&(delete_thread_control.delete_op_sem));
    nanosleep(&time_to_sleep,NULL);
    continue;
   }
  return;
 }

void init_dsync_control()
 {
  memset(&dsync_thread_control,0,sizeof(DSYNC_THREAD_CONTROL));
  sem_init(&(dsync_thread_control.dsync_op_sem),0,1);
  sem_init(&(dsync_thread_control.dsync_queue_sem),0,MAX_DSYNC_CONCURRENCY);
  memset(&(dsync_thread_control.dsync_threads_in_use),0,sizeof(ino_t)*MAX_DSYNC_CONCURRENCY);
  memset(&(dsync_thread_control.dsync_threads_created),0,sizeof(char)*MAX_DSYNC_CONCURRENCY);
  dsync_thread_control.total_active_dsync_threads = 0;

  pthread_create(&(dsync_thread_control.dsync_handler_thread),NULL,(void *)&collect_finished_dsync_threads, NULL);

  return;
 }

void init_delete_control()
 {
  int count,ret_val;

  memset(&delete_thread_control,0,sizeof(DELETE_THREAD_CONTROL));
  memset(&delete_curl_handles,0,sizeof(CURL_HANDLE)*MAX_DELETE_CONCURRENCY);
  for(count=0;count<MAX_DELETE_CONCURRENCY;count++)
   {
    ret_val = hcfs_init_backend(&(delete_curl_handles[count]));
    while ((ret_val < 200) || (ret_val > 299))
     {
      if (delete_curl_handles[count].curl !=NULL)
       hcfs_destroy_backend(delete_curl_handles[count].curl);
      ret_val = hcfs_init_backend(&(delete_curl_handles[count]));
     }

   }

  sem_init(&(delete_thread_control.delete_op_sem),0,1);
  sem_init(&(delete_thread_control.delete_queue_sem),0,MAX_DELETE_CONCURRENCY);
  memset(&(delete_thread_control.delete_threads_in_use),0,sizeof(char)*MAX_DELETE_CONCURRENCY);
  memset(&(delete_thread_control.delete_threads_created),0,sizeof(char)*MAX_DELETE_CONCURRENCY);
  delete_thread_control.total_active_delete_threads = 0;

  pthread_create(&(delete_thread_control.delete_handler_thread),NULL,(void *)&collect_finished_delete_threads, NULL);

  return;
 }


void dsync_single_inode(DSYNC_THREAD_TYPE *ptr)
 {
  char thismetapath[400];
  ino_t this_inode;
  FILE *metafptr;
  FILE_META_TYPE tempfilemeta;
  BLOCK_ENTRY_PAGE temppage;
  int which_curl;
  long long block_no, current_entry_index;
  long long page_pos;
  long long count, block_count;
  unsigned char block_status;
  char delete_done;
  char in_sync;
  int ret_val;
  struct timespec time_to_sleep;

  time_to_sleep.tv_sec = 0;
  time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

  this_inode = ptr->inode;

  fetch_todelete_path(thismetapath,this_inode);

  metafptr=fopen(thismetapath,"r+");
  if (metafptr == NULL)
   {
    return;
   }

  setbuf(metafptr,NULL);

  if ((ptr->this_mode) & S_IFREG)
   {
    flock(fileno(metafptr),LOCK_EX);
    fseek(metafptr,sizeof(struct stat),SEEK_SET);
    fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,metafptr);
    page_pos=tempfilemeta.next_block_page;
    current_entry_index = 0;

    flock(fileno(metafptr),LOCK_UN);

    for(block_count=0;page_pos!=0;block_count++)
     {
      flock(fileno(metafptr),LOCK_EX);

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

      if ((block_status != ST_LDISK) && (block_status != ST_NONE))
       {
        flock(fileno(metafptr),LOCK_UN);
        sem_wait(&(delete_thread_control.delete_queue_sem));
        sem_wait(&(delete_thread_control.delete_op_sem));
        which_curl = -1;
        for(count=0;count<MAX_DELETE_CONCURRENCY;count++)
         {
          if (delete_thread_control.delete_threads_in_use[count] == FALSE)
           {
            delete_thread_control.delete_threads_in_use[count] = TRUE;
            delete_thread_control.delete_threads_created[count] = FALSE;
            delete_thread_control.delete_threads[count].is_block = TRUE;
            delete_thread_control.delete_threads[count].inode = ptr->inode;
            delete_thread_control.delete_threads[count].blockno = block_count;
            delete_thread_control.delete_threads[count].which_curl = count;

            delete_thread_control.total_active_delete_threads++;
            which_curl = count;
            break;
           }
         }
        sem_post(&(delete_thread_control.delete_op_sem));
        pthread_create(&(delete_thread_control.delete_threads_no[which_curl]),NULL, (void *)&con_object_dsync,(void *)&(delete_thread_control.delete_threads[which_curl]));
        delete_thread_control.delete_threads_created[which_curl]=TRUE;
       }
      else
       flock(fileno(metafptr),LOCK_UN);

      current_entry_index++;
     }
/* Block deletion should be done here. Check if all delete threads for this inode has returned before starting meta deletion*/

    delete_done = FALSE;
    while(delete_done == FALSE)
     {
      nanosleep(&time_to_sleep,NULL);
      delete_done = TRUE;
      sem_wait(&(delete_thread_control.delete_op_sem));
      for(count=0;count<MAX_DELETE_CONCURRENCY;count++)
       {
        if ((delete_thread_control.delete_threads_in_use[count] == TRUE) && (delete_thread_control.delete_threads[count].inode == ptr->inode))
         {
          delete_done = FALSE;
          break;
         }
       }
      sem_post(&(delete_thread_control.delete_op_sem));
     }
   }


  sem_wait(&(delete_thread_control.delete_queue_sem));
  sem_wait(&(delete_thread_control.delete_op_sem));
  which_curl = -1;
  for(count=0;count<MAX_DELETE_CONCURRENCY;count++)
   {
    if (delete_thread_control.delete_threads_in_use[count] == FALSE)
     {
      delete_thread_control.delete_threads_in_use[count] = TRUE;
      delete_thread_control.delete_threads_created[count] = FALSE;
      delete_thread_control.delete_threads[count].is_block = FALSE;
      delete_thread_control.delete_threads[count].inode = ptr->inode;
      delete_thread_control.delete_threads[count].which_curl = count;
      delete_thread_control.total_active_delete_threads++;
      which_curl = count;
      break;
     }
   }
  sem_post(&(delete_thread_control.delete_op_sem));

  flock(fileno(metafptr),LOCK_EX);

  pthread_create(&(delete_thread_control.delete_threads_no[which_curl]),NULL, (void *)&con_object_dsync,(void *)&(delete_thread_control.delete_threads[which_curl]));
  delete_thread_control.delete_threads_created[which_curl] = TRUE;
  flock(fileno(metafptr),LOCK_UN);
  fclose(metafptr);

  pthread_join(delete_thread_control.delete_threads_no[which_curl],NULL);

  sem_wait(&(delete_thread_control.delete_op_sem));
  delete_thread_control.delete_threads_in_use[which_curl] = FALSE;
  delete_thread_control.delete_threads_created[which_curl] = FALSE;
  delete_thread_control.total_active_delete_threads--;
  sem_post(&(delete_thread_control.delete_op_sem));
  sem_post(&(delete_thread_control.delete_queue_sem));


/*Wait for any upload to complete and change super inode from to_delete to deleted*/

  while(TRUE)
   {
    in_sync = FALSE;
    sem_wait(&(sync_thread_control.sync_op_sem));
    /*Check if this inode is being synced now*/
     for(count=0;count<MAX_SYNC_CONCURRENCY;count++)
      {
       if (sync_thread_control.sync_threads_in_use[count]==this_inode)
        {
         in_sync = TRUE;
         break;
        }
      }
    sem_post(&(sync_thread_control.sync_op_sem));
    if (in_sync == TRUE)
     sleep(10);
    else
     break;
   }
  unlink(thismetapath);
  super_inode_delete(this_inode);
  super_inode_reclaim();

  return;
 }


void do_meta_delete(ino_t this_inode, CURL_HANDLE *curl_handle)
 {
  char objname[1000];
  int ret_val;

  sprintf(objname,"meta_%lld",this_inode);
  printf("Debug meta deletion: objname %s, inode %lld\n",objname,this_inode);
  sprintf(curl_handle->id,"delete_meta_%lld",this_inode);
  ret_val = hcfs_delete_object(objname, curl_handle);
  return;
 }

void con_object_dsync(DELETE_THREAD_TYPE *delete_thread_ptr)
 {
  int which_curl;
  which_curl = delete_thread_ptr->which_curl;
  if (delete_thread_ptr->is_block == TRUE)
   do_block_delete(delete_thread_ptr->inode, delete_thread_ptr->blockno, &(delete_curl_handles[which_curl]));
  else
   do_meta_delete(delete_thread_ptr->inode, &(delete_curl_handles[which_curl]));

  return;
 }



void delete_loop()
 {
  ino_t inode_to_dsync, inode_to_check;
  DSYNC_THREAD_TYPE dsync_threads[MAX_DSYNC_CONCURRENCY];
  SUPER_INODE_ENTRY tempentry;
  int count,sleep_count;
  char in_dsync;
  int ret_val;

  init_delete_control();
  init_dsync_control();

  printf("Start delete loop\n");

  while(TRUE)
   {
    sleep(5);
    inode_to_check = 0;
    while(TRUE)
     {
      sem_wait(&(dsync_thread_control.dsync_queue_sem));
      sem_wait(&(sys_super_inode->io_sem));
      if (inode_to_check == 0)
       inode_to_check = sys_super_inode->head.first_to_delete_inode;
      inode_to_dsync = 0;
      if (inode_to_check !=0)
       {
        inode_to_dsync = inode_to_check;

        ret_val = read_super_inode_entry(inode_to_dsync,&tempentry);

        if ((ret_val < 0) || (tempentry.status!=TO_BE_DELETED))
         {
          inode_to_dsync = 0;
          inode_to_check = 0;
         }
        else
         {
          inode_to_check = tempentry.util_ll_next;
         }
       }
      sem_post(&(sys_super_inode->io_sem));
      
      if (inode_to_dsync!=0)
       {
        sem_wait(&(dsync_thread_control.dsync_op_sem));
        /*First check if this inode is actually being dsynced now*/
        in_dsync = FALSE;
        for(count=0;count<MAX_DSYNC_CONCURRENCY;count++)
         {
          if (dsync_thread_control.dsync_threads_in_use[count]==inode_to_dsync)
           {
            in_dsync = TRUE;
            break;
           }
         }

        if (in_dsync == FALSE)
         {
          for(count=0;count<MAX_DSYNC_CONCURRENCY;count++)
           {
            if (dsync_thread_control.dsync_threads_in_use[count]==0)
             {
              dsync_thread_control.dsync_threads_in_use[count]=inode_to_dsync;
              dsync_thread_control.dsync_threads_created[count]=FALSE;
              dsync_threads[count].inode = inode_to_dsync;
              dsync_threads[count].this_mode = tempentry.inode_stat.st_mode;
              pthread_create(&(dsync_thread_control.inode_dsync_thread[count]),NULL, (void *)&dsync_single_inode,(void *)&(dsync_threads[count]));
              dsync_thread_control.dsync_threads_created[count]=TRUE;
              dsync_thread_control.total_active_dsync_threads++;
              break;
             }
           }
          sem_post(&(dsync_thread_control.dsync_op_sem));
         }
        else  /*If already dsyncing to cloud*/
         {
          sem_post(&(dsync_thread_control.dsync_op_sem));
          sem_post(&(dsync_thread_control.dsync_queue_sem));
         }        
       }
      else
       {      
        sem_post(&(dsync_thread_control.dsync_queue_sem));
       }
      if (inode_to_check == 0)
       break;
     }
   }
  return;
 }


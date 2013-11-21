/*TODO: When queueing blocks and meta of an inode for upload, dequeue super inode from IS_DIRTY. If at the end of meta upload the meta is moved back to IS_DIRTY, then the content has been updated in between thus it will be scheduled for upload again */
/*TODO: need to debug
1. How to make delete file and upload file work together
TODO: add a "delete from cloud" sequence after the sequence of upload to cloud.
TODO: Will need to consider the case when system restarted or broken connection during meta or ock upload
TODO: multiple upload connections for multiple files
TODO: Will need to be able to delete files or truncate files while it is being synced to cloud or involved in cache replacement
TODO: Track if init_swift may not able to connect and terminate process.
TODO: put super_inode_delete here at delete sequence and run super_inode_reclaim after delete batch is done
TODO: If deleting a block, first check if the meta is there then check if the blocks are reused by checking the length of meta
TODO: Perhaps should compact the deletion of blocks and/or meta in one file into one single entry in the queue
TODO: If compact delete requests in one entry, and only blocks are deleted, can scan the meta to see if blocks are reused but stored locally only. If so, can still delete
*/

#include "hcfs_tocloud.h"
#include "params.h"
#include "hcfscurl.h"
#include "super_inode.h"

CURL_HANDLE upload_curl_handles[MAX_UPLOAD_CONCURRENCY];

void collect_finished_sync_threads(void *ptr)
 {
  int count;
  int ret_val;
  while(TRUE)  /*TODO: Perhaps need to change this flag to allow terminating at shutdown*/
   {
    sem_wait(&(sync_thread_control.sync_op_sem);

    if (sync_thread_control.total_active_sync_threads <=0)
     {
      sem_post(&(sync_thread_control.sync_op_sem);
      sleep(1);
      continue;
     }
    for(count=0;count<MAX_SYNC_CONCURRENCY;count++)
     {
      if (sync_thread_control.sync_threads_in_use[count]!=0)
       {
        ret_val = pthread_tryjoin_np(sync_thread_control.inode_sync_thread[count],NULL);
        if (ret_val == 0)
         {
          sync_thread_control.sync_threads_in_use[count]=0;
          sync_thread_control.total_active_sync_threads --;
          sem_post(&(sync_thread_control.sync_queue_sem));
         }
       }
     }
    sem_post(&(sync_thread_control.sync_op_sem);
    sleep(1);
    continue;
   }
  return;
 }

void init_sync_control()
 {
  sem_init(&(sync_thread_control.sync_op_sem),0,1);
  sem_init(&(sync_thread_control.sync_queue_sem),0,MAX_SYNC_CONCURRENCY);
  memset(&(sync_thread_control.sync_threads_in_use),0,sizeof(ino_t)*MAX_SYNC_CONCURRENCY);
  sync_thread_control.total_active_sync_threads = 0;
  sync_thread_control.total_active_sync_threads = 0;

  pthread_create(&(sync_thread_control.sync_handler_thread),NULL,(void *)&collect_finished_sync_threads, NULL);

  return;
 }

void sync_single_inode(SYNC_THREAD_TYPE *ptr)
 {
  char thismetapath[400];
  ino_t this_inode;
  FILE *metafptr;
  FILE_META_TYPE tempfilemeta;
  BLOCK_ENTRY_PAGE temppage;
  int which_curl;
  long page_pos,block_no, current_entry_index;
  long total_blocks,total_pages;
  long count;
  SUPER_INODE_ENTRY temp_entry;

  this_inode = ptr->inode;

  fetch_meta_path(thismetapath,this_inode);

  metafptr=fopen(thismetapath,"r+");
  setbuf(metafptr,NULL);

  if ((ptr->this_mode) & S_ISREG)
   {
    flock(fileno(metafptr),LOCK_EX);
    fread(&tempfilemeta,sizeof(FILE_META_TYPE),1,metafptr);
    page_pos=tempfilemeta.next_block_page;
    current_entry_index = 0;
    if (tempfilemeta.thisstat.st_size == 0)
     total_blocks = 0;
    else
     total_blocks = ((tempfilemeta.thisstat.st_size - 1) / MAX_BLOCK_SIZE) + 1;

    if (total_blocks ==0)
     total_pages = 0;
    else
     total_pages = ((total_blocks - 1) / MAX_BLOCK_ENTRIES_PER_PAGE) + 1;

    flock(fileno(metafptr),LOCK_UN);

    for(count=0;count<total_blocks;count++)
     {
      flock(fileno(metafptr),LOCK_EX);

      if (current_entry_index >= BLOCK_ENTRY_PAGE)
       {
        page_pos = temppage.next_page;
        current_entry_index = 0;
        if (page_pos == 0)
         {
          flock(fileno(metafptr),LOCK_UN);
          break;
         }
       }

      fseek(metafptr,page_pos,SEEK_SET);
      fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);

      block_status = temppage.block_entries[current_entry_index].status;
      flock(fileno(metafptr),LOCK_UN);

      if ((block_status == ST_LDISK) || (block_status == ST_LtoC))
       {
        sem_wait(&(upload_thread_control.upload_queue_sem));
        sem_wait(&(upload_thread_control.upload_op_sem));
        which_curl = -1;
        for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
         {
          if (upload_thread_control.upload_threads_in_use[count] == FALSE)
           {
            upload_thread_control.upload_threads_in_use[count] = TRUE;
            upload_thread_control.upload_threads[count].is_block = TRUE;
            upload_thread_control.upload_threads[count].inode = ptr->inode;
            upload_thread_control.upload_threads[count].blockno = count;
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

      current_entry_index++;
     }
/* Block sync should be done here. Check if all upload threads for this inode has returned before starting meta sync*/

    upload_done = FALSE;
    while(upload_done == FALSE)
     {
      sleep(1);
      upload_done = TRUE;
      sem_wait(&(upload_thread_control.upload_op_sem));
      for(count = 0;count<MAX_UPLOAD_CONCURRENCY;count++)
       {
        if ((upload_thread_control.upload_threads_in_use[count] == TRUE) && (upload_thread_control.upload_threads[count].inode == ptr->inode)
         {
          upload_done = FALSE;
          break;
         }
       }
      sem_post(&(upload_thread_control.upload_op_sem));
     }
   }

  sem_wait(&(upload_thread_control.upload_queue_sem));
  sem_wait(&(upload_thread_control.upload_op_sem));
  which_curl = -1;
  for(count=0;count<MAX_UPLOAD_CONCURRENCY;count++)
   {
    if (upload_thread_control.upload_threads_in_use[count] == FALSE)
     {
      upload_thread_control.upload_threads_in_use[count] = TRUE;
      upload_thread_control.upload_threads[count].is_block = FALSE;
      upload_thread_control.upload_threads[count].inode = ptr->inode;
      upload_thread_control.upload_threads[count].which_curl = count;
      upload_thread_control.total_active_upload_threads++;
      which_curl = count;
      break;
     }
   }
  sem_post(&(upload_thread_control.upload_op_sem));

  flock(fileno(metafptr),LOCK_EX);
  schedule_sync_meta(metafptr,which_curl); /*Should first copy meta file to a tmp file to avoid inconsistency, then start the upload thread*/
  flock(fileno(metafptr),LOCK_UN);
  fclose(metafptr);

  pthread_join(upload_thread_control.upload_threads_no[which_curl]);
  sem_wait(&(upload_thread_control.upload_op_sem));
  upload_thread_control.upload_threads_in_use[which_curl] = FALSE;
  upload_thread_control.total_active_upload_threads--;
  sem_post(&(upload_thread_control.upload_op_sem));
  sem_post(&(upload_thread_control.upload_queue_sem));

  super_inode_update_transit(ptr->inode,FALSE);

  return;
 }



> 
> 
> void do_block_sync(ino_t this_inode, long block_no, CURL *curl);
> 
> void con_block_sync(upload_thread_type *upload_thread_ptr)
---
>   do_block_sync(upload_thread_ptr->inode, upload_thread_ptr->blockno, upload_thread_ptr->curlptr);
---
> void do_meta_sync(FILE *fptr,char *orig_meta_path,ino_t this_inode, CURL *curl)
---
>   char objname[1000];
> 
>   sprintf(objname,"meta_%ld",this_inode);
>   //printf("Debug datasync: syncing inode number %ld\n",this_inode);
>   swift_put_object(fptr,objname, curl);
---
> void do_block_sync(ino_t this_inode, long block_no, CURL *curl)

>   char objname[1000];
>   char blockpath[1000];

> 
>   sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,(this_inode + block_no) % SYS_DIR_WIDTH,this_inode,block_no);
>   printf("Debug datasync: syncing inode number %ld, block number %ld\n",this_inode, block_no);
>   sprintf(objname,"data_%ld_%ld",this_inode,block_no);
>   printf("Debug datasync: objname %s, inode %ld, block %ld\n",objname,this_inode,block_no);
>   fptr=fopen(blockpath,"r");
>   swift_put_object(fptr,objname, curl);
>   fclose(fptr);
>   return;
>  }
> 
> void fetch_from_cloud(FILE *fptr, ino_t this_inode, long block_no)
>  {
>   char objname[1000];
>   int status;
>   int which_curl_handle;
> 
>   sprintf(objname,"data_%ld_%ld",this_inode,block_no);
>   while(1==1)
>    {
>     sem_wait(&download_curl_sem);
>     for(which_curl_handle=0;which_curl_handle<MAX_CURL_HANDLE;which_curl_handle++)
>      {
>       if (curl_handle_mask[which_curl_handle] == False)
>        {
>         curl_handle_mask[which_curl_handle] = True;
>         break;
>        }
>      }
>     printf("Debug datasync: downloading using curl handle %d\n",which_curl_handle);
>     status=swift_get_object(fptr,objname,download_curl_handles[which_curl_handle].curl);
>     curl_handle_mask[which_curl_handle] = False;
>     sem_post(&download_curl_sem);
>     if (status!=0)
>      {
>       if (swift_reauth()!=0)
>        {
>         printf("Debug datasync: error in connecting to swift\n");
>         exit(0);
>        }
>      }
>     else
>      break;
>    }
> 
>   fflush(fptr);
>   return;
>  }
> 
> 
> void run_maintenance_loop()
>  {

>   size_t super_inode_size;
>   int sleep_count;
>   CURL_HANDLE uploadcurl[max_concurrency];
>   int con_count,total_con_upload;
>   upload_thread_type upload_threads[max_concurrency];
>   pthread_t upload_threads_no[max_concurrency];
---
>   //printf("Start loop\n");
---
>     for (sleep_count=0;sleep_count<30;sleep_count++)
>      {
>       if (mysystem_meta->cache_size < CACHE_SOFT_LIMIT) /*Sleep for a while if we are not really in a hurry*/
>        sleep(1);
>       else
>        break;
>      }
> 
>     for (con_count = 0; con_count < max_concurrency; con_count ++)
>      {
>       if (init_swift_backend(&(uploadcurl[con_count]))!=0)
>        {
>         printf("Debug datasync: error in connecting to swift\n");
>         break;
>        }
>      }
> 
>     //printf("Debug datasync: running syncing\n");

>       //printf("Debug datasync: Check sync\n");
>       sem_wait(super_inode_write_sem);
---
>       super_inode_size = fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
>       sem_post(super_inode_write_sem);
>       if (super_inode_size != 1)
---
>         //printf("Debug datasync: sync with wrong super inode entry size %ld\n",super_inode_size);
>         break;
>        }
>       //printf("Debug datasync: Inode %ld\n",temp_entry.thisstat.st_ino);
> 
> /* TODO:
> 1. Change the scan to create a list of uploads first, then
> 2. For each entry in the upload list, upload the blocks and meta, but if non-exists, just write a debug (if any) and continue
> 3. If files / directories are deleted after meta/block is opened, worst case is that the data is uploaded, then deleted when
> the delete sequence is called up.
> */
>       if ((temp_entry.thisstat.st_ino>0) && ((temp_entry.is_dirty == True) || (temp_entry.in_transit == True)))
>        {
>         //printf("Debug datasync: Inode %ld needs syncing\n",temp_entry.thisstat.st_ino);
55a174,187
>         sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);
>         metaptr=fopen(metapath,"r+");
>         sem_wait(super_inode_write_sem);
> 
>         fseek(super_inode_sync_fptr,thispos,SEEK_SET);
>         fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
> 
>         temp_entry.is_dirty = False;
>         temp_entry.in_transit = True;  /* Adding in_transit flag to avoid interruption during transition */
>         fseek(super_inode_sync_fptr,thispos,SEEK_SET);
>         fwrite(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
>         sem_post(super_inode_write_sem);
> 
> 
---
>           fseek(metaptr,sizeof(struct stat),SEEK_SET);
---
> 
>           count = 0;
>           while(count < total_blocks)
---
>             for (con_count = 0; con_count < max_concurrency;)
>              {
>               if (count >= total_blocks)
>                break;
>             
>             //printf("Debug datasync: Check sync 2\n");
>               flock(fileno(metaptr),LOCK_EX);
>               blockflagpos=ftell(metaptr);
>               fread(&temp_block_entry,sizeof(blockent),1,metaptr);
>               flock(fileno(metaptr),LOCK_UN);
>               printf("Debug datasync: Checking block %ld, stored where %d\n",count+1,temp_block_entry.stored_where);
>               if ((temp_block_entry.stored_where==1) || (temp_block_entry.stored_where==4))
>                {
>                 temp_block_entry.stored_where=4;
>                 printf("datasync.c Debug stored_where changed to 4, block %ld\n",count+1);
>                 fseek(metaptr,blockflagpos,SEEK_SET);
>                 flock(fileno(metaptr),LOCK_EX);
>                 fwrite(&temp_block_entry,sizeof(blockent),1,metaptr);
>                 fflush(metaptr);
>                 flock(fileno(metaptr),LOCK_UN);
>                 upload_threads[con_count].filepos = blockflagpos;
>                 upload_threads[con_count].inode = this_inode;
>                 upload_threads[con_count].blockno = count+1;
>                 upload_threads[con_count].curlptr = uploadcurl[con_count].curl;
>                 pthread_create(&(upload_threads_no[con_count]),NULL, (void *)&con_block_sync,(void *)&(upload_threads[con_count]));
>                 con_count ++;
>                }
>               count ++;
>              }
>             total_con_upload = con_count;
>             
>             for (con_count = 0; con_count < total_con_upload; con_count++)
---
>               pthread_join(upload_threads_no[con_count],NULL);
>               blockflagpos = upload_threads[con_count].filepos;
> 
> //              do_block_sync(this_inode,count+1, uploadcurl.curl);
>               /*First will need to check if the block is modified again*/
>               //printf("Debug datasync: preparing to update block status\n");
>               flock(fileno(metaptr),LOCK_EX);
---
>               fread(&temp_block_entry,sizeof(blockent),1,metaptr);
>               if (temp_block_entry.stored_where==4)
>                {
>                 //printf("Debug datasync: updated block status\n");
>                 temp_block_entry.stored_where=3;
>                 printf("datasync.c Debug stored_where changed to 3, block %ld\n",upload_threads[con_count].blockno);
>                 fseek(metaptr,blockflagpos,SEEK_SET);
>                 fwrite(&temp_block_entry,sizeof(blockent),1,metaptr);
>                 fflush(metaptr);
>                }
>               flock(fileno(metaptr),LOCK_UN);
>               //printf("Debug datasync: finished block update\n");
---
>         flock(fileno(metaptr),LOCK_EX);
>         do_meta_sync(metaptr,metapath,this_inode, uploadcurl[0].curl);
>         flock(fileno(metaptr),LOCK_UN);
>         fclose(metaptr);
> 
>         sem_wait(super_inode_write_sem);
>         fseek(super_inode_sync_fptr,thispos,SEEK_SET);
>         fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
>         temp_entry.in_transit = False;

>         sem_post(super_inode_write_sem);
> 
---
>     //printf("Debug datasync: End running syncing\n");
>     for(con_count=0;con_count<max_concurrency;con_count++)
>      destroy_swift_backend(uploadcurl[con_count].curl);


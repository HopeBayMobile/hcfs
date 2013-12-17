#include "hcfs_cache.h"
#include "params.h"
#include "fuseop.h"
#include "super_inode.h"

/*TODO: For scanning caches, only need to check one block subfolder a time, and scan for mtime
greater than the last update time for uploads, and scan for atime for cache replacement*/

/*TODO: Now pick victims with small inode number. Will need to implement something smarter.*/
/*Only kick the blocks that's stored on cloud, i.e., stored_where ==ST_BOTH*/
/* TODO: Something better for checking if the inode have cache to be kicked out. Will need to consider whether to force checking of replacement? */
void run_cache_loop()
 {
  ino_t this_inode;
  long count,count2,current_block, total_blocks, pagepos, nextpagepos;
  SUPER_INODE_ENTRY tempentry;
  char thismetapath[400];
  char thisblockpath[400];
  FILE *metafptr;
  BLOCK_ENTRY_PAGE temppage;
  int ret_val, current_page_index;
  FILE_META_TYPE temphead;
  struct stat tempstat;
  struct timeval builttime,currenttime;
  long seconds_slept;
  int current_entry_index;
  char skip_recent, do_something;
  time_t node_time;
  CACHE_USAGE_NODE *this_cache_node;

  build_cache_usage();
  gettimeofday(&builttime,NULL);
  current_entry_index=0;  /*Index for doing the round robin in cache dropping*/
  skip_recent = TRUE;
  do_something = FALSE;

  while(1==1)
   {
    seconds_slept = 0;

    while(hcfs_system->systemdata.cache_size >= CACHE_SOFT_LIMIT)
     {
      if (nonempty_cache_hash_entries <=0)
       {
        build_cache_usage();
        gettimeofday(&builttime,NULL);
        current_entry_index=0;
        skip_recent = TRUE;
        do_something = FALSE;
       }

      if (current_entry_index >= CACHE_USAGE_NUM_ENTRIES)
       {
        if ((do_something == FALSE) && (skip_recent == FALSE))
         {
          build_cache_usage();
          gettimeofday(&builttime,NULL);
          current_entry_index=0;
          skip_recent = TRUE;
          do_something = FALSE;
         }
        else
         {
          if ((do_something == FALSE) && (skip_recent == TRUE))
           skip_recent = FALSE;
          current_entry_index = 0;
          do_something = FALSE;
         }
       }

      if (inode_cache_usage_hash[current_entry_index] == NULL)
       {
        current_entry_index++;
        continue;
       }
      if (inode_cache_usage_hash[current_entry_index]->clean_cache_size <=0)
       {
        current_entry_index++;
        continue;
       }

      if (skip_recent == TRUE)
       {
        gettimeofday(&currenttime,NULL);
        node_time = inode_cache_usage_hash[current_entry_index]-> last_access_time;
        if (node_time < inode_cache_usage_hash[current_entry_index]-> last_mod_time)
         node_time = inode_cache_usage_hash[current_entry_index]-> last_mod_time;
        if ((currenttime.tv_sec - node_time) < 300)
         {
          current_entry_index++;
          continue;
         }
       }
      do_something = TRUE;

      this_inode = inode_cache_usage_hash[current_entry_index]-> this_inode;

      this_cache_node = return_cache_usage_node(inode_cache_usage_hash[current_entry_index]-> this_inode);
      free(this_cache_node);
      current_entry_index++;

      super_inode_read(this_inode, &tempentry);

      /* If inode is not dirty or in transit, or if cache is already full, check if can replace uploaded blocks */

/*TODO: if hard limit not reached, perhaps should not throw out blocks so aggressively and can sleep for a while*/
      if ((tempentry.inode_stat.st_ino>0) && (tempentry.inode_stat.st_mode & S_IFREG)) 
       {
        fetch_meta_path(thismetapath,this_inode);
        metafptr=fopen(thismetapath,"r+");
        if (metafptr == NULL)
         continue;

        setbuf(metafptr,NULL);
        flock(fileno(metafptr),LOCK_EX);
        if (access(thismetapath,F_OK)<0)
         {
          /*If meta file does not exist, do nothing*/
          flock(fileno(metafptr),LOCK_UN);
          fclose(metafptr);
          continue;
         }

        current_block = 0;

        fread(&temphead,sizeof(FILE_META_TYPE),1,metafptr);
        nextpagepos = temphead.next_block_page;
        total_blocks = (temphead.thisstat.st_size + (MAX_BLOCK_SIZE -1)) / MAX_BLOCK_SIZE;

        current_page_index = MAX_BLOCK_ENTRIES_PER_PAGE;

        for(current_block = 0;current_block<total_blocks;current_block++)
         {
          if (current_page_index >= MAX_BLOCK_ENTRIES_PER_PAGE)
           {
            if (nextpagepos == 0)
             break;
            pagepos = nextpagepos;
            fseek(metafptr,pagepos,SEEK_SET);
            ret_val = fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
            if (ret_val < 1)
             break;
            nextpagepos = temppage.next_page;

            current_page_index = 0;
           }

          if (temppage.block_entries[current_page_index].status == ST_BOTH)
           {
          /*Only delete blocks that exists on both cloud and local*/
            temppage.block_entries[current_page_index].status = ST_CLOUD;

            printf("Debug status changed to ST_CLOUD, block %ld, inode %ld\n",current_block,this_inode);
            fseek(metafptr,pagepos,SEEK_SET);
            ret_val = fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
            if (ret_val < 1)
             break;
            fetch_block_path(thisblockpath,this_inode,current_block);

            stat(thisblockpath,&tempstat);
            sem_wait(&(hcfs_system->access_sem));
            hcfs_system->systemdata.cache_size -= tempstat.st_size;
            hcfs_system->systemdata.cache_blocks--;
            unlink(thisblockpath);
            sync_hcfs_system_data(FALSE);
            sem_post(&(hcfs_system->access_sem));           
            super_inode_mark_dirty(this_inode);
           }
/*Adding a delta threshold to avoid thrashing at hard limit boundary*/
          if (hcfs_system->systemdata.cache_size < (CACHE_HARD_LIMIT - CACHE_DELTA))
           notify_sleep_on_cache();
          if (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT)
           {
            flock(fileno(metafptr),LOCK_UN);
            while(hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT)
             {
              gettimeofday(&currenttime,NULL);
              /*Rebuild cache usage every five minutes if cache usage not near full*/
              if (((currenttime.tv_sec-builttime.tv_sec) > 300) || (seconds_slept > 300))
               break;
              sleep(1);
              seconds_slept++;
             }
            if ((hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT) &&
                (((currenttime.tv_sec-builttime.tv_sec) > 300) || (seconds_slept > 300)))
             break;


            flock(fileno(metafptr),LOCK_EX);
            if (access(thismetapath,F_OK)<0)
             {
              /*If meta file does not exist, do nothing*/
              break;
             }

            fseek(metafptr,pagepos,SEEK_SET);
            ret_val = fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,metafptr);
            if (ret_val < 1)
             break;
            nextpagepos = temppage.next_page;
           }
          current_page_index++;
         }

        flock(fileno(metafptr),LOCK_UN);
        fclose(metafptr);
       }
     }

    while (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT)
     {
      gettimeofday(&currenttime,NULL);
      /*Rebuild cache usage every five minutes if cache usage not near full*/
      if (((currenttime.tv_sec-builttime.tv_sec) > 300) || (seconds_slept > 300))
       {
        build_cache_usage();
        gettimeofday(&builttime,NULL);
        seconds_slept=0;
        current_entry_index=0;
        skip_recent = TRUE;
        do_something = FALSE;
       }
      sleep(1);
      seconds_slept++;
     }

    
   }
 }

void sleep_on_cache_full()  /*Routine for sleeping threads/processes on cache full*/
 {
  sem_post(&(hcfs_system->num_cache_sleep_sem));
  sem_wait(&(hcfs_system->check_cache_sem));
  sem_wait(&(hcfs_system->num_cache_sleep_sem));
  sem_post(&(hcfs_system->check_next_sem));

  return;
 }

void notify_sleep_on_cache()  /*Routine for waking threads/processes on cache not full*/
 {
  int num_cache_sleep_sem_value;

  while(1==1)
   {
    sem_getvalue(&(hcfs_system->num_cache_sleep_sem),&num_cache_sleep_sem_value);
    if (num_cache_sleep_sem_value > 0) /*If still have threads/processes waiting on cache not full*/
     {
      sem_post(&(hcfs_system->check_cache_sem));
      sem_wait(&(hcfs_system->check_next_sem));
     }
    else
     break;
   }
  return;
 }


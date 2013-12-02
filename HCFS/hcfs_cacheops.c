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

  while(1==1)
   {
    while (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT)
     {
      sleep(1);
     }

    for(count = 1;count<=sys_super_inode->head.num_total_inodes;count++)
     {
      if (hcfs_system->systemdata.cache_size < CACHE_SOFT_LIMIT)
       break;

      for(count2=0;count2<30;count2++)
       {
        if (hcfs_system->systemdata.cache_size >= CACHE_HARD_LIMIT)
         break;
        sleep(1);
       }
      this_inode = count;
      super_inode_read(this_inode, &tempentry);


      /* If inode is not dirty or in transit, or if cache is already full, check if can replace uploaded blocks */

/*TODO: Need to consider last access time and download time to prevent downloaded blocks being
thrown out immediately*/
/*TODO: if hard limit not reached, perhaps should not throw out blocks so aggressively and can sleep for a while*/
      if (((tempentry.inode_stat.st_ino>0) && (tempentry.inode_stat.st_mode & S_IFREG)) 
             && (((tempentry.status != IS_DIRTY) && (tempentry.in_transit == FALSE)) || (hcfs_system->systemdata.cache_size >= CACHE_HARD_LIMIT)))
       {
        fetch_meta_path(thismetapath,this_inode);
        metafptr=fopen(thismetapath,"r+");
        if (metafptr == NULL)
         continue;

        setbuf(metafptr,NULL);
        flock(fileno(metafptr),LOCK_EX);
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
           break;
          current_page_index++;
         }

        flock(fileno(metafptr),LOCK_UN);
        fclose(metafptr);
       }
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


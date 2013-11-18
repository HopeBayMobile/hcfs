#include "hcfs_cache.h"
#include "params.h"

/*TODO: For scanning caches, only need to check one block subfolder a time, and scan for mtime
greater than the last update time for uploads, and scan for atime for cache replacement*/

/*TODO: Now pick victims with small inode number. Will need to implement something smarter.*/
/*Only kick the blocks that's stored on cloud, i.e., stored_where ==3*/
/* TODO: Something better for checking if the inode have cache to be kicked out. Will need to consider whether to force checking of replacement? */
void run_cache_loop()
 {
  FILE *super_inode_sync_fptr;
  FILE *metaptr;
  long current_inodes;
  super_inode_entry temp_entry;
  char superinodepath[400];
  char metapath[400];
  char blockpath[400];
  long thispos, blockflagpos;
  long count,total_blocks;
  ino_t this_inode;
  blockent temp_block_entry;
  struct stat tempstat;
  unsigned char changed;
  size_t super_inode_size;

  sprintf(superinodepath,"%s/%s", METASTORE,"superinodefile");

  super_inode_sync_fptr=fopen(superinodepath,"r+");
  setbuf(super_inode_sync_fptr,NULL);

  while(1==1)
   {
    /*Sleep if not triggered*/
    while (mysystem_meta->cache_size < CACHE_SOFT_LIMIT)
     {
//      printf("not doing cache replacement\n");
      sleep(1);
     }

    fseek(super_inode_sync_fptr,0,SEEK_SET);
    while(!feof(super_inode_sync_fptr))
     {
//      printf("Current cache size is: %ld\n", mysystem_meta->cache_size);
      if (mysystem_meta->cache_size < CACHE_SOFT_LIMIT)
       break;
      sem_wait(super_inode_write_sem);
      thispos=ftell(super_inode_sync_fptr);
//      printf("Debug cache loop: current super inode file pos is %ld\n",thispos);
      super_inode_size = fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
//      printf("Read entry size is %ld\n",super_inode_size);
      sem_post(super_inode_write_sem);

      if (super_inode_size != 1)
       break;

      /* If inode is not dirty or in transit, or if cache is already full, check if can replace uploaded blocks */
      if (((temp_entry.thisstat.st_ino>0) && (temp_entry.thisstat.st_mode & S_IFREG)) 
             && (((temp_entry.is_dirty == False) && (temp_entry.in_transit == False)) || (mysystem_meta->cache_size >= CACHE_HARD_LIMIT)))
       {
        this_inode=temp_entry.thisstat.st_ino;
        sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);
//        printf("Cache checking %s\n",metapath);
        metaptr=fopen(metapath,"r+");
        setbuf(metaptr,NULL);
//        printf("test1\n");
        flock(fileno(metaptr),LOCK_EX);
//        printf("test2\n");
        fseek(metaptr,sizeof(struct stat),SEEK_SET);
        fread(&total_blocks,sizeof(long),1,metaptr);
        changed = False;
//        printf("Cache checking %s\n",metapath);
        for(count=0;count<total_blocks;count++)
         {
          blockflagpos=ftell(metaptr);
          fread(&temp_block_entry,sizeof(blockent),1,metaptr);
//          printf("block no %ld is in where %d\n",(count+1),temp_block_entry.stored_where);
          if (temp_block_entry.stored_where==3)  /*Only delete blocks that exists on both cloud and local*/
           {
            temp_block_entry.stored_where=2;
            printf("Debug stored_where changed to 2, block %ld, inode %ld\n",count+1,this_inode);
            fseek(metaptr,blockflagpos,SEEK_SET);
            fwrite(&temp_block_entry,sizeof(blockent),1,metaptr);
            sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,(this_inode + (count+1)) % SYS_DIR_WIDTH,this_inode, (count+1));
            stat(blockpath,&tempstat);
            sem_wait(mysystem_meta_sem);
            mysystem_meta->cache_size -= tempstat.st_size;
            unlink(blockpath);
            sem_post(mysystem_meta_sem);
            changed = True;
           }
          if (mysystem_meta->cache_size < CACHE_HARD_LIMIT)
           notify_sleep_on_cache();
          if (mysystem_meta->cache_size < CACHE_SOFT_LIMIT)
           break;
         }

        if (changed == True)
         {     /* Need to mark meta as dirty in super inode */
          sem_wait(super_inode_write_sem);
          fseek(super_inode_sync_fptr,thispos,SEEK_SET);
          fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
          temp_entry.is_dirty = True;
          fseek(super_inode_sync_fptr,thispos,SEEK_SET);
          fwrite(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
          sem_post(super_inode_write_sem);
         }
        flock(fileno(metaptr),LOCK_UN);
        fclose(metaptr);
       }
     }

    
   }
 }

void sleep_on_cache_full()  /*Routine for sleeping threads/processes on cache full*/
 {
  sem_post(num_cache_sleep_sem);
  sem_wait(check_cache_sem);
  sem_wait(num_cache_sleep_sem);
  sem_post(check_next_sem);

  return;
 }

void notify_sleep_on_cache()  /*Routine for waking threads/processes on cache not full*/
 {
  int num_cache_sleep_sem_value;

  while(1==1)
   {
    sem_getvalue(num_cache_sleep_sem,&num_cache_sleep_sem_value);
    if (num_cache_sleep_sem_value > 0) /*If still have threads/processes waiting on cache not full*/
     {
      sem_post(check_cache_sem);
      sem_wait(check_next_sem);
     }
    else
     break;
   }
  return;
 }


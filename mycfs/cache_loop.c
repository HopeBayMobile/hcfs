#include "myfuse.h"
#include "mycurl.h"

/*TODO: Now pick victims with small inode number. Will need to implement something smarter.*/
/*Only kick the blocks that's stored on cloud, i.e., stored_where ==3*/
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
    /*Sleep for 10 seconds if not triggered*/
    if (mysystem_meta->cache_size < CACHE_SOFT_LIMIT)
     {
      printf("not doing cache replacement\n");
      sleep(10);
      continue;
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


      if (((temp_entry.thisstat.st_ino>0) && ((temp_entry.is_dirty == False) && (temp_entry.in_transit == False))) && (temp_entry.thisstat.st_mode & S_IFREG))
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
            printf("Debug stored_where changed to 2, block %ld\n",count+1);
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

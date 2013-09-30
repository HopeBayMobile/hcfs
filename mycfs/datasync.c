#include "myfuse.h"
#include "mycurl.h"

/*TODO: need to debug
1. How to make delete file and upload file work together
2. How to handle racing condition in large files
TODO: add a "delete from cloud" sequence after the sequence of upload to cloud.
TODO: Will need to consider the case when system restarted or broken connection during meta or block upload
TODO: multiple connections to backend
TODO: Will need to be able to delete files or truncate files while it is being synced to cloud or involved in cache replacement
*/

void do_meta_sync(FILE *fptr,char *orig_meta_path,ino_t this_inode)
 {
  char objname[1000];

  sprintf(objname,"meta_%ld",this_inode);
  //printf("Debug datasync: syncing inode number %ld\n",this_inode);
  swift_put_object(fptr,objname);
  return;
 }

void do_block_sync(ino_t this_inode, long block_no)
 {
  char objname[1000];
  char blockpath[1000];
  FILE *fptr;

  sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,(this_inode + block_no) % SYS_DIR_WIDTH,this_inode,block_no);
  //printf("Debug datasync: syncing inode number %ld, block number %ld\n",this_inode, block_no);
  sprintf(objname,"data_%ld_%ld",this_inode,block_no);
  fptr=fopen(blockpath,"r");
  swift_put_object(fptr,objname);
  fclose(fptr);
  return;
 }

void fetch_from_cloud(FILE *fptr, ino_t this_inode, long block_no)
 {
  char objname[1000];
  int status;

  sprintf(objname,"data_%ld_%ld",this_inode,block_no);
  while(1==1)
   {
    status=swift_get_object(fptr,objname);
    if (status!=0)
     {
      if (init_swift_backend()!=0)
       {
        printf("Debug datasync: error in connecting to swift\n");
        exit(0);
       }
     }
    else
     break;
   }

  fflush(fptr);
  return;
 }


void run_maintenance_loop()
 {
  FILE *super_inode_sync_fptr;
  FILE *metaptr;
  long current_inodes;
  super_inode_entry temp_entry;
  struct timeb currenttime;
  char printedtime[100];
  char superinodepath[400];
  char metapath[400];
  long thispos, blockflagpos;
  long count,total_blocks;
  ino_t this_inode;
  blockent temp_block_entry;
  size_t super_inode_size;

  sprintf(superinodepath,"%s/%s", METASTORE,"superinodefile");

  super_inode_sync_fptr=fopen(superinodepath,"r+");
  setbuf(super_inode_sync_fptr,NULL);

  //printf("Start loop\n");

  while (1==1)
   {
    if (mysystem_meta->cache_size < CACHE_SOFT_LIMIT) /*Sleep for a while if we are not really in a hurry*/
     sleep(10);

    if (init_swift_backend()!=0)
     {
      printf("Debug datasync: error in connecting to swift\n");
      break;
     }

    //printf("Debug datasync: running syncing\n");
    ftime(&currenttime);
    fseek(super_inode_sync_fptr,0,SEEK_SET);
    while(!feof(super_inode_sync_fptr))
     {
      //printf("Debug datasync: Check sync\n");
      sem_wait(super_inode_write_sem);
      thispos=ftell(super_inode_sync_fptr);
      super_inode_size = fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
      sem_post(super_inode_write_sem);
      if (super_inode_size != 1)
       {
        //printf("Debug datasync: sync with wrong super inode entry size %ld\n",super_inode_size);
        break;
       }
      //printf("Debug datasync: Inode %ld\n",temp_entry.thisstat.st_ino);

/* TODO:
1. Change the scan to create a list of uploads first, then
2. For each entry in the upload list, upload the blocks and meta, but if non-exists, just write a debug (if any) and continue
3. If files / directories are deleted after meta/block is opened, worst case is that the data is uploaded, then deleted when
the delete sequence is called up.
*/
      if ((temp_entry.thisstat.st_ino>0) && ((temp_entry.is_dirty == True) || (temp_entry.in_transit == True)))
       {
        //printf("Debug datasync: Inode %ld needs syncing\n",temp_entry.thisstat.st_ino);
        this_inode=temp_entry.thisstat.st_ino;
        sprintf(metapath,"%s/sub_%ld/meta%ld",METASTORE,this_inode % SYS_DIR_WIDTH,this_inode);
        metaptr=fopen(metapath,"r+");
        sem_wait(super_inode_write_sem);

        fseek(super_inode_sync_fptr,thispos,SEEK_SET);
        fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);

        temp_entry.is_dirty = False;
        temp_entry.in_transit = True;  /* Adding in_transit flag to avoid interruption during transition */
        fseek(super_inode_sync_fptr,thispos,SEEK_SET);
        fwrite(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
        sem_post(super_inode_write_sem);


        if (temp_entry.thisstat.st_mode & S_IFREG)        /*If regular file, need to check which block is dirty*/
         {
          fseek(metaptr,sizeof(struct stat),SEEK_SET);
          fread(&total_blocks,sizeof(long),1,metaptr);
          for(count=0;count<total_blocks;count++)
           {
            //printf("Debug datasync: Check sync 2\n");
            flock(fileno(metaptr),LOCK_EX);
            blockflagpos=ftell(metaptr);
            fread(&temp_block_entry,sizeof(blockent),1,metaptr);
            flock(fileno(metaptr),LOCK_UN);
            //printf("Debug datasync: Checking block %ld, stored where %d\n",count+1,temp_block_entry.stored_where);
            if ((temp_block_entry.stored_where==1) || (temp_block_entry.stored_where==4))
             {
              temp_block_entry.stored_where=4;
              printf("datasync.c Debug stored_where changed to 4, block %ld\n",count+1);
              fseek(metaptr,blockflagpos,SEEK_SET);
              flock(fileno(metaptr),LOCK_EX);
              fwrite(&temp_block_entry,sizeof(blockent),1,metaptr);
              fflush(metaptr);
              flock(fileno(metaptr),LOCK_UN);
              do_block_sync(this_inode,count+1);
              /*First will need to check if the block is modified again*/
              //printf("Debug datasync: preparing to update block status\n");
              flock(fileno(metaptr),LOCK_EX);
              fseek(metaptr,blockflagpos,SEEK_SET);
              fread(&temp_block_entry,sizeof(blockent),1,metaptr);
              if (temp_block_entry.stored_where==4)
               {
                //printf("Debug datasync: updated block status\n");
                temp_block_entry.stored_where=3;
                printf("datasync.c Debug stored_where changed to 3, block %ld\n",count+1);
                fseek(metaptr,blockflagpos,SEEK_SET);
                fwrite(&temp_block_entry,sizeof(blockent),1,metaptr);
                fflush(metaptr);
               }
              flock(fileno(metaptr),LOCK_UN);
              //printf("Debug datasync: finished block update\n");
             }
           }
         }
        flock(fileno(metaptr),LOCK_EX);
        do_meta_sync(metaptr,metapath,this_inode);
        flock(fileno(metaptr),LOCK_UN);
        fclose(metaptr);

        sem_wait(super_inode_write_sem);
        fseek(super_inode_sync_fptr,thispos,SEEK_SET);
        fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
        temp_entry.in_transit = False;
        fseek(super_inode_sync_fptr,thispos,SEEK_SET);
        fwrite(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
        sem_post(super_inode_write_sem);

       }
     }
    ftime(&currenttime);
    //printf("Debug datasync: End running syncing\n");
    destroy_swift_backend();
   }

  return;
 }

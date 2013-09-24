#include "myfuse.h"
#include "mycurl.h"

/*TODO: need to debug
1. How to make delete file and upload file work together
2. How to handle racing condition in large files
TODO: locking in block upload / download
TODO: add a "delete from cloud" sequence after the sequence of upload to cloud.
TODO: Will need to consider the case when system restarted or broken connection during meta or block upload
*/

void do_meta_sync(FILE *fptr,char *orig_meta_path,ino_t this_inode)
 {
  char objname[1000];

  sprintf(objname,"meta_%ld",this_inode);
  printf("syncing inode number %ld\n",this_inode);
  swift_put_object(fptr,objname);
  return;
 }

void do_block_sync(ino_t this_inode, long block_no)
 {
  char objname[1000];
  char blockpath[1000];
  FILE *fptr;

  sprintf(blockpath,"%s/sub_%ld/data_%ld_%ld",BLOCKSTORE,(this_inode + block_no) % SYS_DIR_WIDTH,this_inode,block_no);
  printf("syncing inode number %ld, block number %ld\n",this_inode, block_no);
  sprintf(objname,"data_%ld_%ld",this_inode,block_no);
  fptr=fopen(blockpath,"r");
  swift_put_object(fptr,objname);
  fclose(fptr);
  return;
 }

void run_maintenance_loop()
 {
  FILE *fptr;
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

  sprintf(superinodepath,"%s/%s", METASTORE,"superinodefile");

  fptr=fopen("data_sync_log","w");
  setbuf(fptr,NULL);
  super_inode_sync_fptr=fopen(superinodepath,"r+");
  setbuf(super_inode_sync_fptr,NULL);

  printf("Start loop\n");

  while (1==1)
   {
    if (mysystem_meta.cache_size < CACHE_SOFT_LIMIT) /*Sleep for a while if we are not really in a hurry*/
     sleep(10);
    if (init_swift_backend()!=0)
     {
      printf("error in connecting to swift\n");
      break;
     }
    printf("Debug running syncing\n");
    ftime(&currenttime);
    fseek(super_inode_sync_fptr,0,SEEK_SET);
    while(!feof(super_inode_sync_fptr))
     {
      sem_wait(super_inode_write_sem);
      thispos=ftell(super_inode_sync_fptr);
      fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
      sem_post(super_inode_write_sem);

/* TODO:
1. Change the scan to create a list of uploads first, then
2. For each entry in the upload list, upload the blocks and meta, but if non-exists, just write a debug (if any) and continue
3. If files / directories are deleted after meta/block is opened, worst case is that the data is uploaded, then deleted when
the delete sequence is called up.
*/
      if ((temp_entry.thisstat.st_ino>0) && ((temp_entry.is_dirty == True) || (temp_entry.in_transit == True)))
       {
        fprintf(fptr,"Inode %ld needs syncing\n",temp_entry.thisstat.st_ino);
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
            flock(fileno(metaptr),LOCK_EX);
            blockflagpos=ftell(metaptr);
            fread(&temp_block_entry,sizeof(blockent),1,metaptr);
            flock(fileno(metaptr),LOCK_UN);
            printf("Checking block %ld, stored where %d\n",count+1,temp_block_entry.stored_where);
            if ((temp_block_entry.stored_where==1) || (temp_block_entry.stored_where==4))
             {
              temp_block_entry.stored_where=4;
              fseek(metaptr,blockflagpos,SEEK_SET);
              flock(fileno(metaptr),LOCK_EX);
              fwrite(&temp_block_entry,sizeof(blockent),1,metaptr);
              fflush(metaptr);
              flock(fileno(metaptr),LOCK_UN);
              do_block_sync(this_inode,count+1);
              temp_block_entry.stored_where=3;
              fseek(metaptr,blockflagpos,SEEK_SET);
              flock(fileno(metaptr),LOCK_EX);
              fwrite(&temp_block_entry,sizeof(blockent),1,metaptr);
              fflush(metaptr);
              flock(fileno(metaptr),LOCK_UN);
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
    printf("End running syncing\n");
    destroy_swift_backend();
   }

  return;
 }

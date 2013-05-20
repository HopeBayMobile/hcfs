#include "myfuse.h"

void run_maintenance_loop()
 {
  FILE *fptr;
  FILE *super_inode_sync_fptr;
  long current_inodes;
  super_inode_entry temp_entry;
  struct timeb currenttime;
  char printedtime[100];
  char superinodepath[400];
  long thispos;
  long count;

  sprintf(superinodepath,"%s/%s", METASTORE,"superinodefile");

  fptr=fopen("data_sync_log","w");
  setbuf(fptr,NULL);
  super_inode_sync_fptr=fopen(superinodepath,"r+");
  setbuf(super_inode_sync_fptr,NULL);

  printf("Start loop\n");

  while (1==1)
   {
    sleep(10);
    count = 0;
    printf("Debug running fake syncing\n");
    sem_wait(super_inode_read_sem);
    sem_wait(super_inode_write_sem);
    ftime(&currenttime);
    fprintf(fptr,"Start dumping fake sync log at %s\n",ctime_r(&(currenttime.time),printedtime));
    fseek(super_inode_sync_fptr,0,SEEK_SET);
    while(!feof(super_inode_sync_fptr))
     {
      thispos=ftell(super_inode_sync_fptr);
      fread(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
      printf("%ld\n",count);
      if ((temp_entry.thisstat.st_ino>0) && (temp_entry.is_dirty == True))      
       {
        fprintf(fptr,"Inode %ld needs syncing\n",temp_entry.thisstat.st_ino);
        temp_entry.is_dirty = False;
        fseek(super_inode_sync_fptr,thispos,SEEK_SET);
        fwrite(&temp_entry,sizeof(super_inode_entry),1,super_inode_sync_fptr);
       }
      count++;
     }
    ftime(&currenttime);
    fprintf(fptr,"Finished dumping fake sync log at %s\n",ctime_r(&(currenttime.time),printedtime));
    sem_post(super_inode_write_sem);
    sem_post(super_inode_read_sem);
    printf("End running fake syncing\n");
   }

  return;
 }

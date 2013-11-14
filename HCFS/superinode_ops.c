#include "fuseop.h"
#include "super_inode.h"
#include "params.h"
#include <sys/ipc.h>
#include <sys/shm.h>

int super_inode_init()
 {
  int shm_key;

  shm_key = shmget(1234,sizeof(SUPER_INODE_CONTROL), IPC_CREAT| 0666);
  sys_super_inode = shmat(shm_key, NULL, 0);

  memset(sys_super_inode,0,sizeof(SUPER_INODE_CONTROL));
  sem_init(&(sys_super_inode->io_sem),1,1);
  
  sys_super_inode->iofptr = fopen(SUPERINODE,"r+");
  if (sys_super_inode->iofptr == NULL)
   {
    sys_super_inode->iofptr = fopen(SUPERINODE,"w+");
    fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);
    fclose(sys_super_inode->iofptr);
    sys_super_inode->iofptr = fopen(SUPERINODE,"r+");
   }
  setbuf(sys_super_inode->iofptr,NULL);
  
  fread(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);

  return 0;
 }

int super_inode_destroy()
 {
  sem_wait(&(sys_super_inode->io_sem));
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);
  sem_post(&(sys_super_inode->io_sem));

  return 0;
 }

int super_inode_read(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;
  int ret_items;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fread(inode_ptr,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
   }
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }
int super_inode_write(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;
  int ret_items;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  inode_ptr->is_dirty = TRUE;
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fwrite(inode_ptr,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
   }
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }

int super_inode_update_stat(ino_t this_inode, struct stat *newstat)
 {
  int ret_val;
  int ret_items;
  SUPER_INODE_ENTRY tempentry;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  memset(&tempentry,0,sizeof(SUPER_INODE_ENTRY));
  memcpy(&(tempentry.inode_stat),newstat,sizeof(struct stat));
  tempentry.is_dirty = TRUE;
  tempentry.this_index = newstat->st_ino;  

  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
   }
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }


int super_inode_to_delete(ino_t this_inode)
 {
  int ret_val;
  int ret_items;
  SUPER_INODE_ENTRY tempentry;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
    else
     {
      tempentry.to_be_deleted = TRUE;
      tempentry.to_be_reclaimed = FALSE;
      tempentry.next_reclaimed_inode = 0;
      tempentry.is_dirty = FALSE;
      memset(&(tempentry.inode_stat),0,sizeof(struct stat));
     }
   }
  if (ret_val>=0)
   {
    fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
    if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
     ret_val = -1;
    else
     {
      ret_items=fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
      if (ret_items<1)
       ret_val = -1;
     }
   }

  sys_super_inode->head.num_active_inodes--;
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);

  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }

int super_inode_delete(ino_t this_inode)
 {
  int ret_val;
  int ret_items;
  SUPER_INODE_ENTRY tempentry;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
    else
     {
      tempentry.to_be_deleted = FALSE;
      tempentry.to_be_reclaimed = TRUE;
      tempentry.next_reclaimed_inode = 0;
      tempentry.is_dirty = FALSE;
      memset(&(tempentry.inode_stat),0,sizeof(struct stat));
     }
   }
  if (ret_val>=0)
   {
    fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
    if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
     ret_val = -1;
    else
     {
      ret_items=fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
      if (ret_items<1)
       ret_val = -1;
     }
   }

  sys_super_inode->head.num_to_be_reclaimed++;
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);

  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }


int super_inode_reclaim(int fullscan)
 {
  long total_inodes_reclaimed;
  int ret_val,ret_items;
  SUPER_INODE_ENTRY tempentry;
  long count;
  long thisfilepos;
  ino_t last_reclaimed,first_reclaimed,old_last_reclaimed;

  if ((fullscan == FALSE) && (sys_super_inode->head.num_to_be_reclaimed < RECLAIM_TRIGGER))
   return 0;
  last_reclaimed = 0;
  first_reclaimed = 0;

  ret_val = 0;

  sem_wait(&(sys_super_inode->io_sem));
  if (fullscan == TRUE)
   sys_super_inode->head.num_inode_reclaimed = 0;
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD),SEEK_SET);
  for(count=0;count<sys_super_inode->head.num_total_inodes;count++)
   {
    if (fullscan==FALSE)
     {
      if (sys_super_inode->head.num_to_be_reclaimed<=0)
       break;
     }
    thisfilepos=ftell(sys_super_inode->iofptr);
    ret_items=fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     break;
    if ((tempentry.to_be_reclaimed == TRUE) || ((fullscan == TRUE) && ((tempentry.inode_stat.st_ino ==0) && (tempentry.to_be_deleted == FALSE))))
     {
      tempentry.to_be_reclaimed = FALSE;
      sys_super_inode->head.num_inode_reclaimed++;
      sys_super_inode->head.num_to_be_reclaimed--;
      tempentry.next_reclaimed_inode = 0;
      fseek(sys_super_inode->iofptr,thisfilepos,SEEK_SET);
      ret_items=fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
      if (ret_items<1)
       break;
      if (first_reclaimed == 0)
       first_reclaimed = tempentry.this_index;
      old_last_reclaimed = last_reclaimed;
      last_reclaimed = tempentry.this_index;

      if (old_last_reclaimed > 0)
       {
        fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (old_last_reclaimed-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
        thisfilepos=ftell(sys_super_inode->iofptr);
        ret_items=fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
        if (ret_items < 1)
         break;
        if (tempentry.this_index != old_last_reclaimed)
         break;
        tempentry.next_reclaimed_inode = last_reclaimed;
        fseek(sys_super_inode->iofptr,thisfilepos,SEEK_SET);
        ret_items=fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
        if (ret_items<1)
         break;
       }
     }
   }

  sys_super_inode->head.first_reclaimed_inode = first_reclaimed;
  sys_super_inode->head.last_reclaimed_inode = last_reclaimed;
  sys_super_inode->head.num_to_be_reclaimed = 0;
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);

  sem_post(&(sys_super_inode->io_sem));
  return ret_val;
 }

ino_t super_inode_new_inode(struct stat *in_stat)
 {
  int ret_items;
  ino_t this_inode;
  SUPER_INODE_ENTRY tempentry;
  struct stat tempstat;
  ino_t new_first_reclaimed;

  sem_wait(&(sys_super_inode->io_sem));

  if (sys_super_inode->head.num_inode_reclaimed > 0)
   {
    this_inode = sys_super_inode->head.first_reclaimed_inode;
    fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
    if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
     {
      sem_post(&(sys_super_inode->io_sem));
      return 0;
     }
    ret_items = fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items < 1)
     {
      sem_post(&(sys_super_inode->io_sem));
      return 0;
     }
    new_first_reclaimed = tempentry.next_reclaimed_inode;
    if (new_first_reclaimed == 0) /*If there are no more reclaimed inode*/
     {
      /*TODO: Need to check if num_inode_reclaimed is 0. If not, need
        to rescan super inode*/
      sys_super_inode->head.num_inode_reclaimed = 0;
      sys_super_inode->head.first_reclaimed_inode = 0;
      sys_super_inode->head.last_reclaimed_inode = 0;
     }
    else /*Update super inode head regularlly*/
     {
      sys_super_inode->head.num_inode_reclaimed--;
      sys_super_inode->head.first_reclaimed_inode = new_first_reclaimed;
     }
    
   }
  else /*If need to append a new super inode and add total inode count*/
   {
    sys_super_inode->head.num_total_inodes++;
    this_inode = sys_super_inode->head.num_total_inodes;
   }
  sys_super_inode->head.num_active_inodes++;

  /*Update the new super inode entry*/
  memset(&tempentry,0,sizeof(SUPER_INODE_ENTRY));
  tempentry.this_index = this_inode;
  tempentry.is_dirty = TRUE;
  memcpy(&tempstat,in_stat,sizeof(struct stat));
  tempstat.st_ino = this_inode;
  memcpy(&(tempentry.inode_stat),&tempstat,sizeof(struct stat));
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   {
    sem_post(&(sys_super_inode->io_sem));
    return 0;
   }
  ret_items = fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
  if (ret_items < 1)
   {
    sem_post(&(sys_super_inode->io_sem));
    return 0;
   }
  /*TODO: Error handling here if write to super inode head failed*/
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);

  sem_post(&(sys_super_inode->io_sem));

  return this_inode;
 }

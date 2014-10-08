/*TODO: Consider to convert super inode to multiple files and use striping for efficiency*/
/*TODO: Consider using multiple FILE handles for super inode IO.*/

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fuseop.h"
#include "global.h"
#include "super_inode.h"
#include "params.h"


int write_super_inode_head()
 {
  int ret_val;
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=0)
   return -1;
  ret_val = fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);
  if (ret_val < 1)
   return -1;
  return 0;
 }

int read_super_inode_entry(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;

  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   return -1;
  ret_val=fread(inode_ptr,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
  if (ret_val<1)
   return -1;
  return 0;
 }


int write_super_inode_entry(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;

  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   return -1;
  ret_val=fwrite(inode_ptr,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
  if (ret_val<1)
   return -1;
  return 0;
 }
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
  sys_super_inode->unclaimed_list_fptr=fopen(UNCLAIMEDFILE,"a+");
  setbuf(sys_super_inode->unclaimed_list_fptr,NULL);
  
  fread(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);

  return 0;
 }

int super_inode_destroy()
 {
  sem_wait(&(sys_super_inode->io_sem));
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);
  fclose(sys_super_inode->iofptr);
  fclose(sys_super_inode->unclaimed_list_fptr);

  sem_post(&(sys_super_inode->io_sem));

  return 0;
 }

int super_inode_read(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;
  int ret_items;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  ret_val = read_super_inode_entry(this_inode, inode_ptr);
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }
int super_inode_write(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;
  int ret_items;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  if (inode_ptr->status != IS_DIRTY)
   {
    ll_dequeue(this_inode,inode_ptr);
    ll_enqueue(this_inode,IS_DIRTY,inode_ptr);
    write_super_inode_head();
   }
  if (inode_ptr -> in_transit == TRUE)
   inode_ptr -> mod_after_in_transit = TRUE;
  ret_val = write_super_inode_entry(this_inode, inode_ptr);

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

  ret_val = read_super_inode_entry(this_inode,&tempentry);
  if (ret_val >=0)
   {
    if (tempentry.status != IS_DIRTY)
     {
      ll_dequeue(this_inode,&tempentry);
      ll_enqueue(this_inode,IS_DIRTY,&tempentry);
      write_super_inode_head();
     }
    if (tempentry.in_transit == TRUE)
     tempentry.mod_after_in_transit = TRUE;

    memcpy(&(tempentry.inode_stat),newstat,sizeof(struct stat));
    ret_val = write_super_inode_entry(this_inode, &tempentry);
   }
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }

int super_inode_mark_dirty(ino_t this_inode)
 {
  int ret_val;
  int ret_items;
  SUPER_INODE_ENTRY tempentry;
  char need_write;

  need_write = FALSE;
  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));

  ret_val = read_super_inode_entry(this_inode,&tempentry);
  if (ret_val >=0)
   {
    if (tempentry.status == NO_LL)
     {
      ll_enqueue(this_inode,IS_DIRTY,&tempentry);
      write_super_inode_head();
      need_write = TRUE;
     }
    if (tempentry.in_transit == TRUE)
     {
      need_write = TRUE;
      tempentry.mod_after_in_transit = TRUE;
     }

    if (need_write == TRUE)
     ret_val = write_super_inode_entry(this_inode, &tempentry);

   }
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }



int super_inode_update_transit(ino_t this_inode, char is_start_transit)
 {
  int ret_val;
  int ret_items;
  SUPER_INODE_ENTRY tempentry;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));

  ret_val = read_super_inode_entry(this_inode,&tempentry);
  if (ret_val >=0)
   {
    if (((is_start_transit == FALSE) && (tempentry.status == IS_DIRTY)) && (tempentry.mod_after_in_transit == FALSE))
     {  /*If finished syncing and no more mod is done after queueing the inode for syncing*/
      /*Remove from is_dirty list*/
      ll_dequeue(this_inode,&tempentry);
      write_super_inode_head();
     }
    tempentry.in_transit = is_start_transit;
    tempentry.mod_after_in_transit = FALSE;
    ret_val = write_super_inode_entry(this_inode, &tempentry);
   }
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }


int super_inode_to_delete(ino_t this_inode)
 {
  int ret_val;
  int ret_items;
  SUPER_INODE_ENTRY tempentry;
  mode_t tempmode;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));

  ret_val = read_super_inode_entry(this_inode,&tempentry);
  if (ret_val >=0)
   {
    if (tempentry.status != TO_BE_DELETED)
     {
      ll_dequeue(this_inode,&tempentry);
      ll_enqueue(this_inode,TO_BE_DELETED,&tempentry);
     }
    tempentry.in_transit = FALSE;
    tempmode = tempentry.inode_stat.st_mode;
    memset(&(tempentry.inode_stat),0,sizeof(struct stat));
    tempentry.inode_stat.st_mode = tempmode;
    ret_val = write_super_inode_entry(this_inode, &tempentry);

    if (ret_val >=0)
     {
      sys_super_inode->head.num_active_inodes--;
      write_super_inode_head();
     }

   }
  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }

int super_inode_delete(ino_t this_inode)
 {
  int ret_val;
  int ret_items;
  SUPER_INODE_ENTRY tempentry;
  ino_t temp;

  ret_val = 0;
  sem_wait(&(sys_super_inode->io_sem));
  ret_val = read_super_inode_entry(this_inode, &tempentry);

  if (ret_val >=0)
   {
    if (tempentry.status != TO_BE_RECLAIMED)
     {
      ll_dequeue(this_inode, &tempentry);
      tempentry.status = TO_BE_RECLAIMED;
     }
    tempentry.in_transit = FALSE;
    memset(&(tempentry.inode_stat),0,sizeof(struct stat));
    ret_val = write_super_inode_entry(this_inode,&tempentry);

   }

  temp=this_inode;
  fseek(sys_super_inode->unclaimed_list_fptr,0,SEEK_END);
  fwrite(&temp,sizeof(ino_t),1,sys_super_inode->unclaimed_list_fptr); 

  sys_super_inode->head.num_to_be_reclaimed++;
  write_super_inode_head();

  sem_post(&(sys_super_inode->io_sem));

  return ret_val;
 }

static int compino(const void *firstino,const void *secondino)
 {
  ino_t temp1,temp2;

  temp1 = * (ino_t *) firstino;
  temp2 = * (ino_t *) secondino;
  if (temp1 > temp2)
   return 1;
  if (temp1 == temp2)
   return 0;
  return -1;
 }

int super_inode_reclaim()
 {
  long long total_inodes_reclaimed;
  int ret_val;
  SUPER_INODE_ENTRY tempentry;
  long long count;
  off_t thisfilepos;
  ino_t last_reclaimed,new_last_reclaimed;
  ino_t *unclaimed_list;
  long long num_unclaimed_in_list;

  last_reclaimed = 0;

  ret_val = 0;

  if (sys_super_inode->head.num_to_be_reclaimed < RECLAIM_TRIGGER)
   return 0;

  sem_wait(&(sys_super_inode->io_sem));

  fseek(sys_super_inode->unclaimed_list_fptr,0,SEEK_END);
  num_unclaimed_in_list = (ftell(sys_super_inode->unclaimed_list_fptr))/(sizeof(ino_t));

  unclaimed_list = malloc(sizeof(ino_t)*num_unclaimed_in_list);
  fseek(sys_super_inode->unclaimed_list_fptr,0,SEEK_SET);

  num_unclaimed_in_list = fread(unclaimed_list,sizeof(ino_t),num_unclaimed_in_list,sys_super_inode->unclaimed_list_fptr);

  /*TODO: Handle the case if the number of inodes in file is different from that in superinode head*/

  qsort(unclaimed_list,num_unclaimed_in_list,sizeof(ino_t),compino);

  last_reclaimed = sys_super_inode->head.first_reclaimed_inode;

  for(count=num_unclaimed_in_list-1;count>=0;count--)
   {
    ret_val = read_super_inode_entry(unclaimed_list[count],&tempentry);
    if (ret_val<0)
     break;

    if (tempentry.status == TO_BE_RECLAIMED)
     {
      if (sys_super_inode->head.last_reclaimed_inode == 0)
       sys_super_inode->head.last_reclaimed_inode = unclaimed_list[count];
      tempentry.status = RECLAIMED;
      sys_super_inode->head.num_inode_reclaimed++;
      tempentry.util_ll_next = last_reclaimed;
      last_reclaimed = unclaimed_list[count];
      sys_super_inode->head.first_reclaimed_inode = last_reclaimed;
      ret_val = write_super_inode_entry(unclaimed_list[count],&tempentry);
      if (ret_val < 0)
       break;
     }
   }

  sys_super_inode->head.num_to_be_reclaimed = 0;

  write_super_inode_head();

  ftruncate(fileno(sys_super_inode->unclaimed_list_fptr),0);

  free(unclaimed_list);
  sem_post(&(sys_super_inode->io_sem));
  return ret_val;
 }


int super_inode_reclaim_fullscan()
 {
  long long total_inodes_reclaimed;
  int ret_val,ret_items;
  SUPER_INODE_ENTRY tempentry;
  long long count;
  off_t thisfilepos;
  ino_t last_reclaimed,first_reclaimed,old_last_reclaimed;

  last_reclaimed = 0;
  first_reclaimed = 0;

  ret_val = 0;

  sem_wait(&(sys_super_inode->io_sem));
  sys_super_inode->head.num_inode_reclaimed = 0;
  sys_super_inode->head.num_to_be_reclaimed = 0;

  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD),SEEK_SET);
  for(count=0;count<sys_super_inode->head.num_total_inodes;count++)
   {
    thisfilepos=ftell(sys_super_inode->iofptr);
    ret_items=fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     break;
    if ((tempentry.status == TO_BE_RECLAIMED) || ((tempentry.inode_stat.st_ino ==0) && (tempentry.status != TO_BE_DELETED)))
     {
      tempentry.status = RECLAIMED;
      sys_super_inode->head.num_inode_reclaimed++;
      tempentry.util_ll_next = 0;
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
        tempentry.util_ll_next = last_reclaimed;
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
    new_first_reclaimed = tempentry.util_ll_next;
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
//  ll_enqueue(this_inode,IS_DIRTY,&tempentry); /*Don't do it here to prevent premature meta upload

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


int ll_enqueue(ino_t thisinode, char which_ll, SUPER_INODE_ENTRY *this_entry)
 {
  SUPER_INODE_ENTRY tempentry;  

  if (this_entry->status == which_ll)
   return 0;
  if (this_entry->status !=NO_LL)
   ll_dequeue(thisinode, this_entry);

  if (which_ll == NO_LL)
   return 0;

  if (which_ll == TO_BE_RECLAIMED)  /*This has its own list*/
   return 0;

  if (which_ll == RECLAIMED)  /*This has its own operations*/
   return 0;

  switch(which_ll)
   {
    case IS_DIRTY:
        if (sys_super_inode->head.first_dirty_inode==0)
         {
          sys_super_inode->head.first_dirty_inode = thisinode;
          sys_super_inode->head.last_dirty_inode = thisinode;
          this_entry -> util_ll_next = 0;
          this_entry -> util_ll_prev = 0;
          sys_super_inode->head.num_dirty ++;
         }
        else
         {
          this_entry -> util_ll_prev = sys_super_inode->head.last_dirty_inode;
          sys_super_inode->head.last_dirty_inode = thisinode;
          this_entry -> util_ll_next = 0;
          sys_super_inode->head.num_dirty ++;
          fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD)+((this_entry->util_ll_prev-1) * sizeof(SUPER_INODE_ENTRY)),SEEK_SET);
          fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
          tempentry.util_ll_next = thisinode;
          fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD)+((this_entry->util_ll_prev-1) * sizeof(SUPER_INODE_ENTRY)),SEEK_SET);
          fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
         }
        break;
    case TO_BE_DELETED:
        if (sys_super_inode->head.first_to_delete_inode==0)
         {
          sys_super_inode->head.first_to_delete_inode = thisinode;
          sys_super_inode->head.last_to_delete_inode = thisinode;
          this_entry -> util_ll_next = 0;
          this_entry -> util_ll_prev = 0;
          sys_super_inode->head.num_to_be_deleted ++;
         }
        else
         {
          this_entry -> util_ll_prev = sys_super_inode->head.last_to_delete_inode;
          sys_super_inode->head.last_to_delete_inode = thisinode;
          this_entry -> util_ll_next = 0;
          sys_super_inode->head.num_to_be_deleted ++;
          fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD)+((this_entry->util_ll_prev-1) * sizeof(SUPER_INODE_ENTRY)),SEEK_SET);
          fread(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
          tempentry.util_ll_next = thisinode;
          fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD)+((this_entry->util_ll_prev-1) * sizeof(SUPER_INODE_ENTRY)),SEEK_SET);
          fwrite(&tempentry,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
         }
        break;
    default: break;
   }

  this_entry -> status = which_ll;
  return 0;
 }



int ll_dequeue(ino_t thisinode, SUPER_INODE_ENTRY *this_entry)
 {
  SUPER_INODE_ENTRY tempentry;  
  char old_which_ll;
  ino_t temp_inode;

  old_which_ll = this_entry->status;

  if (old_which_ll == NO_LL)
   return 0;

  if (old_which_ll == TO_BE_RECLAIMED)  /*This has its own list*/
   return 0;

  if (old_which_ll == RECLAIMED)  /*This has its own operations*/
   return 0;

  if (this_entry->util_ll_next == 0)
   {
    switch(old_which_ll)
     {
      case IS_DIRTY: 
        sys_super_inode->head.last_dirty_inode = this_entry-> util_ll_prev;
        break;
      case TO_BE_DELETED:
        sys_super_inode->head.last_to_delete_inode = this_entry-> util_ll_prev;
        break;
      default: break;
     }
   }
  else
   {
    temp_inode = this_entry->util_ll_next;
    read_super_inode_entry(temp_inode, &tempentry);
    tempentry.util_ll_prev = this_entry->util_ll_prev;
    write_super_inode_entry(temp_inode, &tempentry);
   }

  if (this_entry->util_ll_prev == 0)
   {
    switch(old_which_ll)
     {
      case IS_DIRTY: 
        sys_super_inode->head.first_dirty_inode = this_entry-> util_ll_next;
        break;
      case TO_BE_DELETED:
        sys_super_inode->head.first_to_delete_inode = this_entry-> util_ll_next;
        break;
      default: break;
     }
   }
  else
   {
    temp_inode = this_entry->util_ll_prev;
    read_super_inode_entry(temp_inode, &tempentry);
    tempentry.util_ll_next = this_entry->util_ll_next;
    write_super_inode_entry(temp_inode, &tempentry);
   }

  switch(old_which_ll)
   {
    case IS_DIRTY:
        sys_super_inode->head.num_dirty--;
        break;
    case TO_BE_DELETED:
        sys_super_inode->head.num_to_be_deleted--;
        break;
    default: break;
   }

  this_entry -> status = NO_LL;
  this_entry -> util_ll_next = 0;
  this_entry -> util_ll_prev = 0;
  return 0;
 }


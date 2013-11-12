#include "fuseop.h"
#include "super_inode.h"
#include "params.h"
#include <sys/ipc.h>
#include <sys/shm.h>

int super_inode_init()
 {
  int shm_key;

  shm_key = shmget(1234,sizeof(SUPER_INODE_CONTROL), IPC_CREAT| 0666);
  sys_super_inode = (shm_key, NULL, 0);

  memset(sys_super_inode,0,sizeof(SUPER_INODE_CONTROL));
  sem_init(sys_super_inode->io_sem,1,1);
  
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
  sem_wait(sys_super_inode->io_sem);
  fseek(sys_super_inode->iofptr,0,SEEK_SET);
  fwrite(&(sys_super_inode->head),sizeof(SUPER_INODE_HEAD),1,sys_super_inode->iofptr);
  sem_post(sys_super_inode->io_sem);

  return 0;
 }

int super_inode_read(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;
  int ret_items;

  ret_val = 0;
  sem_wait(sys_super_inode->io_sem);
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fread(inode_ptr,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
   }
  sem_post(sys_super_inode->io_sem);

  return ret_val;
 }
int super_inode_write(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;
  int ret_items;

  ret_val = 0;
  sem_wait(sys_super_inode->io_sem);
  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fwrite(inode_ptr,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
   }
  sem_post(sys_super_inode->io_sem);

  return ret_val;
 }

int super_inode_to_delete(ino_t this_inode);
int super_inode_delete(ino_t this_inode);
int super_inode_reclaim();
ino_t super_inode_new_inode(struct stat *in_stat, SUPER_INODE_ENTRY *inode_ptr)
 {
  int ret_val;
  int ret_items;
  ino_t this_inode;

  ret_val = 0;
  sem_wait(sys_super_inode->io_sem);

  if (sys_super_inode->head.num_inode_reclaimed > 0)
   {
    

  fseek(sys_super_inode->iofptr,sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY),SEEK_SET);
  if (ftell(sys_super_inode->iofptr)!=sizeof(SUPER_INODE_HEAD) + (this_inode-1) * sizeof(SUPER_INODE_ENTRY))
   ret_val = -1;
  else
   {
    ret_items=fwrite(inode_ptr,sizeof(SUPER_INODE_ENTRY),1,sys_super_inode->iofptr);
    if (ret_items<1)
     ret_val = -1;
   }
  sem_post(sys_super_inode->io_sem);

  return ret_val;
 }

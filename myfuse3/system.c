/* Code under development by Jiahong Wu */

#include "myfuse.h"

void initsystem()
 {
  char systemmetapath[400];
  char superinodepath[400];
  int count,count1,count2;

  for(count1=0;count1<SYS_DIR_WIDTH;count1++)
   {
    sprintf(systemmetapath,"%s/sub_%d", METASTORE, count1);
    if (access(systemmetapath,F_OK)!=0)
     mkdir(systemmetapath,0755);
   }

  for(count1=0;count1<SYS_DIR_WIDTH;count1++)
   {
    sprintf(systemmetapath,"%s/sub_%d", BLOCKSTORE, count1);
    if (access(systemmetapath,F_OK)!=0)
     mkdir(systemmetapath,0755);
   }

  sprintf(systemmetapath,"%s/%s", METASTORE,"systemmeta");
  sprintf(superinodepath,"%s/%s", METASTORE,"superinodefile");

  memset(opened_files_masks,0,sizeof(uint64_t)* (MAX_FILE_TABLE_SIZE/64));

  num_opened_files = 0;

  if (access(systemmetapath,F_OK)!=0)
   {
    super_inode_write_fptr=fopen(superinodepath,"w+");
    fclose(super_inode_write_fptr);
    super_inode_write_fptr=fopen(superinodepath,"w+");
    super_inode_read_fptr=fopen(superinodepath,"r+");
    setbuf(super_inode_write_fptr,NULL);  /* Need to set inode pool I/O to unbuf for sync purpose*/
    setbuf(super_inode_read_fptr,NULL);
    create_root_meta();
    fflush(super_inode_write_fptr);
    mysystem_meta.total_inodes=1;
    mysystem_meta.max_inode=1;
    mysystem_meta.system_size=0;
    system_meta_fptr=fopen(systemmetapath,"w+");
    fwrite(&mysystem_meta,sizeof(system_meta),1,system_meta_fptr);
    fflush(system_meta_fptr);
   }
  else
   {
    system_meta_fptr=fopen(systemmetapath,"r+");
    fread(&mysystem_meta,sizeof(system_meta),1,system_meta_fptr);
    super_inode_write_fptr=fopen(superinodepath,"r+");
    super_inode_read_fptr=fopen(superinodepath,"r+");
    setbuf(super_inode_write_fptr,NULL);  /* Need to set inode pool I/O to unbuf for sync purpose*/
    setbuf(super_inode_read_fptr,NULL);
   }
  memset(&path_cache,0,sizeof(path_cache_entry)*(MAX_ICACHE_ENTRY));
  for(count=0;count<MAX_ICACHE_ENTRY;count++)
   sem_init(&(path_cache[count].cache_sem),0,1);
  sem_init(&file_table_sem,0,1);
  sem_init(&super_inode_read_sem,0,1);
  sem_init(&super_inode_write_sem,0,1);
  sem_init(&mysystem_meta_sem,0,1);


  return;
 }

void mysync_system_meta()
 {
  sem_wait(&mysystem_meta_sem);
  fseek(system_meta_fptr,0,SEEK_SET);
  fwrite(&mysystem_meta,sizeof(system_meta),1,system_meta_fptr);
  fflush(system_meta_fptr);
  sem_post(&mysystem_meta_sem);
  return;
 }

void mydestroy(void *private_data)
 {
  mysync_system_meta();
  fclose(system_meta_fptr);
  fclose(super_inode_read_fptr);
  fclose(super_inode_write_fptr);
  return;
 }

void create_root_meta()
 {
  FILE *meta_fptr;
  struct stat rootmeta;
  char metapath[400];
  long num_dir_ent;
  long num_reg_ent;
  simple_dirent ent1,ent2;
  struct timeb currenttime;

  ftime(&currenttime);

  sprintf(metapath,"%s/sub_%d/%s", METASTORE, 1 % SYS_DIR_WIDTH, "meta1");
  memset(&rootmeta,0,sizeof(struct stat));
  rootmeta.st_uid=getuid();
  rootmeta.st_gid=getgid();
  rootmeta.st_nlink=2;
  rootmeta.st_ino=1;
  rootmeta.st_mode=S_IFDIR | 0755;
  rootmeta.st_atime=currenttime.time;
  rootmeta.st_mtime=rootmeta.st_atime;
  rootmeta.st_ctime=rootmeta.st_atime;
  meta_fptr=fopen(metapath,"w");
  fwrite(&rootmeta,sizeof(struct stat),1,meta_fptr);
  fseek(super_inode_write_fptr,0,SEEK_SET);
  fwrite(&rootmeta,sizeof(struct stat),1,super_inode_write_fptr);
  num_dir_ent=2;
  num_reg_ent=0;
  fwrite(&num_dir_ent,sizeof(long),1,meta_fptr);
  fwrite(&num_reg_ent,sizeof(long),1,meta_fptr);
  memset(&ent1,0,sizeof(simple_dirent));
  memset(&ent2,0,sizeof(simple_dirent));
  ent1.st_ino=1;
  ent1.st_mode=S_IFDIR | 0755;
  strcpy(ent1.name,".");
  ent2.st_ino=0;                /* For st_ino < 1, fill in NULL for readdir filler*/
  ent2.st_mode=S_IFDIR | 0755;
  strcpy(ent2.name,"..");
  fwrite(&ent1,sizeof(simple_dirent),1,meta_fptr);
  fwrite(&ent2,sizeof(simple_dirent),1,meta_fptr);
  fclose(meta_fptr);

  return;
 }

int super_inode_read(struct stat *inputstat,ino_t this_inode)
 {
  size_t total_read;

  sem_wait(&(super_inode_read_sem));
  fseek(super_inode_read_fptr,sizeof(struct stat)*(this_inode-1),SEEK_SET);
  total_read=fread(inputstat,sizeof(struct stat),1,super_inode_read_fptr);
  sem_post(&(super_inode_read_sem));

  if ((total_read < 1) || (inputstat->st_ino < 1))
   return -ENOENT;
  return 0;
 }
int super_inode_write(struct stat *inputstat,ino_t this_inode)
 {
  int total_write;

  sem_wait(&(super_inode_write_sem));
  fseek(super_inode_write_fptr,sizeof(struct stat)*(this_inode-1),SEEK_SET);
  total_write=fwrite(inputstat,sizeof(struct stat),1,super_inode_write_fptr);
  sem_post(&(super_inode_write_sem));

  if (total_write < 1)
   return -1;
  return 0;
 }

int super_inode_create(struct stat *inputstat,ino_t this_inode)
 {
  int total_write;

  sem_wait(&(super_inode_write_sem));
  fseek(super_inode_write_fptr,sizeof(struct stat)*(this_inode-1),SEEK_SET);
  total_write=fwrite(inputstat,sizeof(struct stat),1,super_inode_write_fptr);
  sem_post(&(super_inode_write_sem));

  if (total_write < 1)
   return -1;
  return 0;
 }

int super_inode_delete(ino_t this_inode)
 {
  int total_write;
  struct stat inputstat;

  memset(&inputstat,0,sizeof(struct stat));
  sem_wait(&(super_inode_write_sem));
  fseek(super_inode_write_fptr,sizeof(struct stat)*(this_inode-1),SEEK_SET);
  total_write=fwrite(&inputstat,sizeof(struct stat),1,super_inode_write_fptr);
  sem_post(&(super_inode_write_sem));

  if (total_write < 1)
   return -1;
  return 0;
 }


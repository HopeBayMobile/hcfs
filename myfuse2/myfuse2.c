/* Code under development by Jiahong Wu*/

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include <semaphore.h>
#include <sys/timeb.h>
#define METASTORE "/home/dclouduser/myfuse2/metastorage"
#define BLOCKSTORE "/home/dclouduser/myfuse2/blockstorage"
#define MAX_ICACHE_ENTRY 65536
#define MAX_ICACHE_PATHLEN 1024
#define MAX_FILE_TABLE_SIZE 1024

long MAX_BLOCK_SIZE;

typedef struct {
  ino_t st_ino;
  mode_t st_mode;
  char name[256];
 } simple_dirent;

typedef struct {
  ino_t total_inodes;
  ino_t max_inode;
  long system_size;
 } system_meta;

typedef struct {
  int stored_where;  /*0: not stored, 1: local, 2: cloud, 3: local + cloud*/
  long block_index;
 } blockent;

system_meta mysystem_meta;
FILE *system_meta_fptr;

typedef struct {
  char pathname[MAX_ICACHE_PATHLEN];
  ino_t st_ino;
  sem_t cache_sem;
 } path_cache_entry;

path_cache_entry path_cache[MAX_ICACHE_ENTRY];

typedef struct {
  ino_t st_ino;
  FILE *metaptr;
  FILE *blockptr;
  long opened_block;
 } file_handle_entry;

file_handle_entry file_handle_table[MAX_FILE_TABLE_SIZE+1];

uint64_t num_opened_files;
uint64_t opened_files_masks[MAX_FILE_TABLE_SIZE/64];
sem_t file_table_sem;

void create_root_meta();

void show_current_time()
 {
  struct timeb currenttime;
  char printedtime[100];
  
  ftime(&currenttime);  
  printf("%s.%d\n", ctime_r(&(currenttime.time),printedtime),currenttime.millitm);
  return;
 }

unsigned int compute_inode_hash(const char *path)
 {
  int pathlengthi,count;
  unsigned int hash;

  show_current_time();
  hash = 0;

  for(count=0;count<strlen(path);count++)
   {
    hash += ((unsigned int)path[count]) << (count % 15);
    hash = hash % MAX_ICACHE_ENTRY;
   }

  if (hash >= MAX_ICACHE_ENTRY)
   hash = 0;

  printf("Debug path %s, hash %d\n",path,hash);
  return hash;
 }

void replace_inode_cache(unsigned int inodehash,const char *fullpath, ino_t st_ino)
 {
  printf("debug replacing icache %d with %s, inode %ld\n",inodehash, fullpath, st_ino);
  sem_wait(&(path_cache[inodehash].cache_sem));
  strcpy(path_cache[inodehash].pathname,fullpath);
  path_cache[inodehash].st_ino = st_ino;
  sem_post(&(path_cache[inodehash].cache_sem));
 }

void initsystem()
 {
  char systemmetapath[400];
  int count;

  MAX_BLOCK_SIZE = 2*1024*1024;
  sprintf(systemmetapath,"%s/%s", METASTORE,"systemmeta");
  
  memset(opened_files_masks,0,sizeof(uint64_t)* (MAX_FILE_TABLE_SIZE/64));  

  num_opened_files = 0;

  if (access(systemmetapath,F_OK)!=0)
   {
    create_root_meta();
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
   }
  memset(&path_cache,0,sizeof(path_cache_entry)*(MAX_ICACHE_ENTRY));
  for(count=0;count<MAX_ICACHE_ENTRY;count++)
   sem_init(&(path_cache[count].cache_sem),0,1);
  sem_init(&file_table_sem,0,1);

  return;
 } 

void mysync_system_meta()
 {
  fseek(system_meta_fptr,0,SEEK_SET);
  fwrite(&mysystem_meta,sizeof(system_meta),1,system_meta_fptr);
  fflush(system_meta_fptr);
  return;
 }

void mydestroy(void *private_data)
 {
  mysync_system_meta();
  fclose(system_meta_fptr);
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

  sprintf(metapath,"%s/%s", METASTORE,"meta1");
  memset(&rootmeta,0,sizeof(struct stat));
  rootmeta.st_uid=getuid();
  rootmeta.st_gid=getgid();
  rootmeta.st_nlink=2;
  rootmeta.st_ino=1;
  rootmeta.st_mode=S_IFDIR | 0755;
  meta_fptr=fopen(metapath,"w");
  fwrite(&rootmeta,sizeof(struct stat),1,meta_fptr);
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

ino_t find_inode_fullpath(const char *path, ino_t this_inode, const char *fullpath, unsigned int inodehash)
 {
  char *ptr;
  char subname[400];
  FILE *fptr;
  char metapath[1024];
  long num_subdir;
  long num_reg;
  long count;
  simple_dirent tempent[500];
  simple_dirent *tempent_ptr;
  int num_tempent;
  int next_tempent;
  long tempent_index;
  int tempent_toread;

  printf("find inode %s full %s\n",path,fullpath);
  ptr=strchr(path,'/');

  if (ptr==NULL)
   {
    strcpy(subname,path);

    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);

    printf("debug find inode %s\n",metapath);

    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     return 0;
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(&num_subdir, sizeof(long),1,fptr);
    fread(&num_reg, sizeof(long),1,fptr);
//    printf("Parent inode %ld, num sub %ld, num reg %ld\n",this_inode,num_subdir,num_reg);
    num_tempent=0;
    next_tempent=0;
    tempent_index=0;
    for(count=0;count<(num_subdir+num_reg);count++)
     {
      if (next_tempent >= num_tempent)
       {
        next_tempent = 0;
        if (((num_subdir+num_reg)-tempent_index) < 500)
         tempent_toread=((num_subdir+num_reg)-tempent_index);
        else
         tempent_toread = 500;
        tempent_index += tempent_toread;
        num_tempent = tempent_toread;
        fread(&tempent,sizeof(simple_dirent),tempent_toread,fptr);
       }
//      fread(&tempent,sizeof(simple_dirent),1,fptr);
//      printf("sub name %s\n",tempent.name);
      tempent_ptr=&(tempent[next_tempent]);
      next_tempent++;
//      printf("Index %d, num tempent %d, name %s\n",next_tempent-1,num_tempent,tempent_ptr->name);
//      show_current_time();
      if (strcmp(subname,tempent_ptr->name)==0)
       {
        fclose(fptr);
        if (strlen(fullpath)<MAX_ICACHE_PATHLEN)
         replace_inode_cache(inodehash,fullpath,tempent_ptr->st_ino);
        return tempent_ptr->st_ino;
       }
     }
    fclose(fptr);
    return 0;
   }
  else
   {
    strncpy(subname,path,(ptr-path));
    subname[(ptr-path)]=0;

    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);

    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     return 0;
    fseek(fptr,sizeof(struct stat),SEEK_SET);
    fread(&num_subdir, sizeof(long),1,fptr);
    fread(&num_reg, sizeof(long),1,fptr);
    for(count=0;count<num_subdir;count++)
     {
      fread(&(tempent[0]),sizeof(simple_dirent),1,fptr);
      if (strcmp(subname,tempent[0].name)==0)
       {
        fclose(fptr);
        return find_inode_fullpath(ptr+1,tempent[0].st_ino, fullpath, inodehash);
       }
     }
    /* No need to check the reg files. This pathname is a directory.*/
    fclose(fptr);
    return 0;
   }
  return 0;
 }

void invalidate_inode_cache(const char *path)
 {
  unsigned int inodehash;

  if (strcmp(path,"/")==0)
   return;

  printf("debug start invalidate inode cache: path %s\n",path);

  inodehash=compute_inode_hash(path);

  sem_wait(&(path_cache[inodehash].cache_sem));
  if ((path_cache[inodehash].st_ino>0) && (strcmp(path,path_cache[inodehash].pathname)==0))
   {
    path_cache[inodehash].st_ino=0;
    path_cache[inodehash].pathname[0]=0;
    printf("Debug: found and invalidate inode hash entry for %s\n",path);
   }

  sem_post(&(path_cache[inodehash].cache_sem));
  return;
 }

/* TODO: Change this to inodehash style also */
ino_t find_inode(const char *path)
 {
  ino_t this_inode;
  int count;
  char temp_pathname[1024];
  ino_t temp_inode;
  unsigned int inodehash, inodehash_org;

  show_current_time();
  if (strcmp(path,"/")==0)
   return 1;

  printf("find inode start: path %s\n",path);

  inodehash_org = 0;
  if (strlen(path)<MAX_ICACHE_PATHLEN)
   {
    inodehash_org = compute_inode_hash(path);
    sem_wait(&(path_cache[inodehash_org].cache_sem));
    printf("debug hash comp %d: in cache %s, this path %s\n",inodehash,path_cache[inodehash_org].pathname,path);
    if (strcmp(path,path_cache[inodehash_org].pathname)==0)
     {
      printf("Hit, inode %ld\n",path_cache[inodehash_org].st_ino);
      this_inode=path_cache[inodehash_org].st_ino;
      sem_post(&(path_cache[inodehash_org].cache_sem));
      return this_inode;
     }
    sem_post(&(path_cache[inodehash_org].cache_sem));
   }

  for(count=(strlen(path)-1);count>0;count--)
   {
    if ((count >= MAX_ICACHE_PATHLEN) || (path[count]!='/'))
     continue;
    strncpy(temp_pathname,path,count);
    temp_pathname[count]=0;
    inodehash = compute_inode_hash(temp_pathname);

    sem_wait(&(path_cache[inodehash].cache_sem));
    if (path_cache[inodehash].st_ino ==0)
     {
      sem_post(&(path_cache[inodehash].cache_sem));
      continue;
     }

    if (strcmp(path_cache[inodehash].pathname,temp_pathname)==0)
     {
      temp_inode = path_cache[inodehash].st_ino;
      sem_post(&(path_cache[inodehash].cache_sem));
      this_inode=find_inode_fullpath(&path[count+1],temp_inode,path, inodehash_org);
      return this_inode;
     }
    sem_post(&(path_cache[inodehash].cache_sem));
   }
  this_inode=find_inode_fullpath(&path[1],1,path, inodehash_org);
  return this_inode;
 }

static int mygetattr(const char *path, struct stat *nodestat)
 {

  int retcode=0;
  struct stat inputstat;
  ino_t this_inode;
  FILE *fptr;
  char metapath[1024];

  show_current_time();


  memset(nodestat,0,sizeof(struct stat));

  this_inode = find_inode(path);

  printf("Inode is %ld\n",this_inode);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fread((void*)&inputstat,sizeof(struct stat),1,fptr);
      fclose(fptr);
      nodestat->st_nlink=inputstat.st_nlink;
      nodestat->st_uid=inputstat.st_uid;
      nodestat->st_gid=inputstat.st_gid;
      nodestat->st_dev=inputstat.st_dev;
      nodestat->st_ino=inputstat.st_ino;
      nodestat->st_size=inputstat.st_size;
      nodestat->st_blksize=MAX_BLOCK_SIZE;
      nodestat->st_blocks=inputstat.st_blocks;
      nodestat->st_atime=inputstat.st_atime;
      nodestat->st_mtime=inputstat.st_mtime;
      nodestat->st_ctime=inputstat.st_ctime;
      nodestat->st_mode=inputstat.st_mode;
     }

   }

  return retcode;
 }   

static int myreaddir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg,count;
  simple_dirent tempent;

  show_current_time();


  this_inode = find_inode(path);

  if (this_inode <=0)
   {
    retcode = -ENOENT;
   }
  else
   {
    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir, sizeof(long),1,fptr);
      fread(&num_reg, sizeof(long),1,fptr);
      for(count=0;count<num_subdir+num_reg;count++)
       {
        fread(&tempent,sizeof(simple_dirent),1,fptr);

        if (tempent.st_ino>0)
         {
          memset(&inputstat,0,sizeof(struct stat));
          inputstat.st_ino=tempent.st_ino;
          inputstat.st_mode=tempent.st_mode;
          filler(buf, tempent.name, &inputstat, 0);
         }
        else
         filler(buf, tempent.name, NULL, 0);
       }

      fclose(fptr);
     }
   }
  return 0;
 }

static int myopen(const char *path, struct fuse_file_info *fi)
 {
  int count,count2;
  uint64_t empty_index;
  uint64_t temp_mask;
  char metapath[1024];

  printf("Debug open path %s\n",path);
  show_current_time();


  if (num_opened_files > MAX_FILE_TABLE_SIZE)
   {
    fi -> fh = 0;
    return 0;
   }
  
  /*now find the empty file table slot*/
  sem_wait(&file_table_sem);
  empty_index=0;
  for(count=0;count<(MAX_FILE_TABLE_SIZE/64);count++)
   {
    temp_mask = ~opened_files_masks[count];
    if (temp_mask > 0)  /*have empty slot*/
     {
      temp_mask = 1;
      for(count2=0;count2<64;count2++)
       {
        if ((opened_files_masks[count] & temp_mask) ==0)
         {
          empty_index = count*64+count2+1;
          opened_files_masks[count] = opened_files_masks[count] | temp_mask;
          num_opened_files++;
          break;
         }
        if (count2 == 63)
         break;
        temp_mask = temp_mask << 1;
       }
     }
    else
     continue;
    if (empty_index > 0)
     break;
   }

  fi -> fh = empty_index;
  file_handle_table[empty_index].st_ino = find_inode(path);
  if (file_handle_table[empty_index].st_ino==0)
   {
    /*No such inode?*/
    num_opened_files--;
    temp_mask = ~temp_mask;
    opened_files_masks[count] = opened_files_masks[count] & temp_mask;
    fi -> fh =0;
    sem_post(&file_table_sem);
    return -ENOENT;
   }
  show_current_time();

  file_handle_table[empty_index].opened_block=0;
  file_handle_table[empty_index].blockptr=NULL;

  sprintf(metapath,"%s/meta%ld",METASTORE,file_handle_table[empty_index].st_ino);
  file_handle_table[empty_index].metaptr=fopen(metapath,"r+");
  printf("debug open metapath is %s\n",metapath);
  if (file_handle_table[empty_index].metaptr ==NULL)
   {
    /*No such inode?*/
    num_opened_files--;
    temp_mask = ~temp_mask;
    opened_files_masks[count] = opened_files_masks[count] & temp_mask;
    fi -> fh =0;
    sem_post(&file_table_sem);
    return -ENOENT;
   }
  
  sem_post(&file_table_sem);
  printf("Debug end of myopen\n");
  show_current_time();
  return 0;
 }
static int myrelease(const char *path, struct fuse_file_info *fi)
 {
  uint64_t index;
  int count,count2;
  uint64_t temp_mask;

  if (fi->fh == 0)
   return 0;

  sem_wait(&file_table_sem);
  index = fi->fh;
  if (file_handle_table[index].metaptr!=NULL)
   fclose(file_handle_table[index].metaptr);
  if (file_handle_table[index].blockptr!=NULL)
   fclose(file_handle_table[index].blockptr);
  file_handle_table[index].opened_block = 0;
  file_handle_table[index].st_ino = 0;
  file_handle_table[index].metaptr=NULL;
  file_handle_table[index].blockptr=NULL;

  index=index-1;
  count = index / 64;
  count2 = index % 64;

  temp_mask = 1;
  temp_mask = temp_mask << count2;
  temp_mask = ~temp_mask;
  opened_files_masks[count] = opened_files_masks[count] & temp_mask;
  num_opened_files--;

  sem_post(&file_table_sem);
  return 0;
 } 

static int myopendir(const char *path, struct fuse_file_info *fi)
 {
  show_current_time();

  return 0;
 }
static int myread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
 {
  int retsize=0;
  FILE *metaptr,*data_fptr;
  ino_t this_inode;
  char metapath[1024];
  char blockpath[1024];
  struct stat inputstat;
  long total_blocks,start_block,end_block,count;
  int total_read_bytes, max_to_read, have_error, actual_read_bytes;
  off_t current_offset,end_bytes;
  blockent tmp_block;

  show_current_time();

  printf("Debug myread path %s, size %ld, offset %ld\n",path,size,offset);

  this_inode = find_inode(path);
  if (this_inode==0)
   return -ENOENT;

  sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
  metaptr=fopen(metapath,"r+");
  if (metaptr==NULL)
   return -ENOENT;
  fread(&inputstat,sizeof(struct stat),1,metaptr);
  fread(&total_blocks,sizeof(long),1,metaptr);

  if (offset >= inputstat.st_size)  /* If want to read outside file size */
   {
    fclose(metaptr);
    return 0;
   }
  total_read_bytes=0;

  current_offset=offset;
  end_bytes = (long)offset + (long)size;

  start_block = (offset / (long) MAX_BLOCK_SIZE) + 1;
  end_block = (end_bytes / (long) MAX_BLOCK_SIZE) + 1;  /*Assume that block_index starts from 1. */

  printf("%ld, %ld, %ld\n",start_block,end_block,end_bytes);
  printf("total block %ld\n",total_blocks);

  if (start_block > total_blocks)
   {
    fclose(metaptr);
    return 0;
   }

  fseek(metaptr,sizeof(struct stat)+sizeof(long)+(start_block-1)*sizeof(blockent),SEEK_SET); /*Seek to the first block to read on the meta*/

  printf("%ld, %ld\n",start_block,end_block);
  for(count=start_block;count<=end_block;count++)
   {
    if (count > total_blocks)
     {
      /*End of file encountered*/
      break;
     }
    else
     fread(&tmp_block,sizeof(blockent),1,metaptr);
    sprintf(blockpath,"%s/data_%ld_%ld",BLOCKSTORE,this_inode,count);
    if (tmp_block.stored_where==1)
     {
      data_fptr=fopen(blockpath,"r");
     }
    else
     {
      /*Storage in cloud not implemeted now*/
      break;
     }
    printf("%s\n",blockpath);
    fseek(data_fptr,current_offset - (MAX_BLOCK_SIZE * (count-1)),SEEK_SET);
    if ((MAX_BLOCK_SIZE * count) < (offset+size))
     max_to_read = (MAX_BLOCK_SIZE * count) - current_offset;
    else
     max_to_read = (offset+size) - current_offset;
    actual_read_bytes = fread(&buf[current_offset-offset],sizeof(char),max_to_read,data_fptr);
    total_read_bytes += actual_read_bytes;
    current_offset +=actual_read_bytes;
    if (actual_read_bytes < max_to_read)
     {
      have_error = ferror(data_fptr);
      fclose(data_fptr);
      if (have_error>0)
       {
        retsize = -have_error;
        break;
       }
      else
       break;
     }
    else
     fclose(data_fptr);
   }

  if (retsize>=0)
   retsize = total_read_bytes;
  printf("Debug myread end path %s, size %ld, offset %ld, total write %d\n",path,size,offset,retsize);
  fclose(metaptr);

  return retsize;
 }

static int mywrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
 {
  int retsize=0;
  FILE *metaptr,*data_fptr;
  ino_t this_inode;
  char metapath[1024];
  char blockpath[1024];
  struct stat inputstat;
  long total_blocks,start_block,end_block,count;
  int total_write_bytes, max_to_write, have_error, actual_write_bytes;
  off_t current_offset,end_bytes;
  blockent tmp_block;

  show_current_time();

  printf("Debug mywrite path %s, size %ld, offset %ld\n",path,size,offset);

  if (fi->fh==0)
   {
    this_inode = find_inode(path);
    if (this_inode==0)
     return -ENOENT;

    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
    metaptr=fopen(metapath,"r+");
    if (metaptr==NULL)
     return -ENOENT;
   }
  else
   {
    metaptr = file_handle_table[fi->fh].metaptr;
    this_inode = file_handle_table[fi->fh].st_ino;
    fseek(metaptr,0,SEEK_SET);
   }

  fread(&inputstat,sizeof(struct stat),1,metaptr);
  fread(&total_blocks,sizeof(long),1,metaptr);

  total_write_bytes=0;

  current_offset=offset;
  end_bytes = (long)offset + (long)size;

  start_block = (offset / (long) MAX_BLOCK_SIZE) + 1;
  end_block = (end_bytes / (long) MAX_BLOCK_SIZE) + 1;  /*Assume that block_index starts from 1. */

  printf("%ld, %ld, %ld\n",start_block,end_block,end_bytes);
  printf("total block %ld\n",total_blocks);

  if ((start_block -1) > total_blocks)  /*Padding the file meta*/
   {
    memset(&tmp_block,0,sizeof(blockent));
    fseek(metaptr,0,SEEK_END);
    for (count = 0;count<((start_block-1) - total_blocks); count++)
     {
      tmp_block.block_index = (total_blocks+1+count);
      fwrite(&tmp_block,sizeof(blockent),1,metaptr);
     }
    total_blocks += ((start_block -1) - total_blocks);
   }
  fseek(metaptr,sizeof(struct stat)+sizeof(long)+(start_block-1)*sizeof(blockent),SEEK_SET); /*Seek to the first block to write on the meta*/
  printf("%ld, %ld\n",start_block,end_block);
  for(count=start_block;count<=end_block;count++)
   {
    if (count > total_blocks)
     {
      memset(&tmp_block,0,sizeof(blockent));
      total_blocks++;
      tmp_block.block_index = total_blocks;
     }
    else
     fread(&tmp_block,sizeof(blockent),1,metaptr);
    sprintf(blockpath,"%s/data_%ld_%ld",BLOCKSTORE,this_inode,count);
    if ((fi->fh>0) && (file_handle_table[fi->fh].opened_block == count))
     {
      data_fptr=file_handle_table[fi->fh].blockptr;
     }
    else
     {
      if ((fi->fh>0) && (file_handle_table[fi->fh].opened_block!=0))
       {
        fclose(file_handle_table[fi->fh].blockptr);
        file_handle_table[fi->fh].opened_block=0;
       }
      if (tmp_block.stored_where==1)
       {
        data_fptr=fopen(blockpath,"r+");
       }
      else
       {
        data_fptr=fopen(blockpath,"w+");
        tmp_block.stored_where=1;
       }
      if (fi->fh>0)
       {
        file_handle_table[fi->fh].opened_block=count;
        file_handle_table[fi->fh].blockptr=data_fptr;
       }
     }
    printf("%s\n",blockpath);
    fseek(data_fptr,current_offset - (MAX_BLOCK_SIZE * (count-1)),SEEK_SET);
    if ((MAX_BLOCK_SIZE * count) < (offset+size))
     max_to_write = (MAX_BLOCK_SIZE * count) - current_offset;
    else
     max_to_write = (offset+size) - current_offset;
    actual_write_bytes = fwrite(&buf[current_offset-offset],sizeof(char),max_to_write,data_fptr);
    total_write_bytes += actual_write_bytes;
    current_offset +=actual_write_bytes;
    fseek(metaptr,sizeof(struct stat)+sizeof(long)+(count-1)*sizeof(blockent),SEEK_SET);
    fwrite(&tmp_block,sizeof(blockent),1,metaptr);
    if (actual_write_bytes < max_to_write)
     {
      have_error = ferror(data_fptr);
      if (fi->fh>0)
       {
        file_handle_table[fi->fh].opened_block=0;
        file_handle_table[fi->fh].blockptr=NULL;
       }
      fclose(data_fptr);
      if (have_error>0)
       {
        retsize = -have_error;
        break;
       }
      else
       {
        printf("ERROR! Short written but have no ferror...\n");
        retsize = -1;
        break;
       }
     }
    else
     {
      if (fi->fh==0)
       fclose(data_fptr);
     }
   }
    
  if (inputstat.st_size < (offset+total_write_bytes))
   {
    inputstat.st_size = offset+total_write_bytes;
    inputstat.st_blocks = (inputstat.st_size+511)/512;
   }
  fseek(metaptr,0,SEEK_SET);
  fwrite(&inputstat,sizeof(struct stat),1,metaptr);
  fwrite(&total_blocks,sizeof(long),1,metaptr);

  if (retsize>=0)
   retsize = total_write_bytes;
  printf("Debug mywrite end path %s, size %ld, offset %ld, total write %d\n",path,size,offset,retsize); 
  if (fi->fh == 0)
   fclose(metaptr);
  return retsize;
 }

ino_t find_parent_inode(const char *path)
 {
  /* Find the inode of the parent of "path"*/
  ino_t this_inode;
  char *parentname;
  char *tmpptr;

  tmpptr = strrchr(path,'/');
  if (tmpptr==path)
   {
    this_inode=1;
   }
  else
   {
    parentname=malloc((tmpptr-path)+10);
    if (parentname == NULL)
     return 0;
    strncpy(parentname,path,(tmpptr-path));
    parentname[(tmpptr-path)] = 0;

    this_inode = find_inode(parentname);

    free(parentname);
   }

  return this_inode;
 }

static int mymknod(const char *path, mode_t filemode,dev_t thisdev)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode,new_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg,count;
  simple_dirent tempent;
  char *tmpptr;
  long num_blocks =0;
  unsigned int inodehash;

  tmpptr = strrchr(path,'/');
  show_current_time();


  this_inode = find_parent_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
    fptr=fopen(metapath,"r+");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      mysystem_meta.total_inodes +=1;
      mysystem_meta.max_inode+=1;
      new_inode = mysystem_meta.max_inode;
      fseek(fptr,sizeof(struct stat)+sizeof(long),SEEK_SET);
      fread(&num_reg,sizeof(long),1,fptr);
      num_reg++;
      fseek(fptr,sizeof(struct stat)+sizeof(long),SEEK_SET);
      fwrite(&num_reg,sizeof(long),1,fptr);
      fseek(fptr,0,SEEK_END);
      memset(&tempent,0,sizeof(simple_dirent));
      tempent.st_ino = new_inode;
      tempent.st_mode = S_IFREG | 0755;
      strcpy(tempent.name,&path[tmpptr-path+1]);
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);
      fclose(fptr);
      memset(&inputstat,0,sizeof(struct stat));
      inputstat.st_ino=new_inode;
      inputstat.st_mode = S_IFREG | 0755;
      inputstat.st_nlink = 1;
      inputstat.st_uid=getuid();
      inputstat.st_gid=getgid();
      sprintf(metapath,"%s/meta%ld",METASTORE,new_inode);
      fptr=fopen(metapath,"w");
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      num_blocks = 0;
      fwrite(&num_blocks,sizeof(long),1,fptr);
      fclose(fptr);
     }
   }

  if (strlen(path)<MAX_ICACHE_PATHLEN)
   {
    inodehash = compute_inode_hash(path);
    replace_inode_cache(inodehash,path,new_inode);
   }
  mysync_system_meta();
  show_current_time();

  return retcode;
 }

static int mymkdir(const char *path,mode_t thismode)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t this_inode,new_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg,count;
  simple_dirent tempent,ent1,ent2;
  char *tmpptr;

  show_current_time();

  tmpptr = strrchr(path,'/');

  this_inode = find_parent_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
    fptr=fopen(metapath,"r+");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      mysystem_meta.total_inodes +=1;
      mysystem_meta.max_inode+=1;
      new_inode = mysystem_meta.max_inode;
      fread(&inputstat,sizeof(struct stat),1,fptr);
      inputstat.st_nlink++;
      fseek(fptr,0,SEEK_SET);
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir,sizeof(long),1,fptr);
      fread(&num_reg,sizeof(long),1,fptr);
      num_subdir++;
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite(&num_subdir,sizeof(long),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_subdir-1)*sizeof(simple_dirent)),SEEK_SET);
      if (num_reg > 0)    /* We will need to swap the first regular file entry to the end of meta*/
       {
        fread(&tempent,sizeof(simple_dirent),1,fptr);
        fseek(fptr,0,SEEK_END);
        fwrite(&tempent,sizeof(simple_dirent),1,fptr);
        fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_subdir-1)*sizeof(simple_dirent)),SEEK_SET);
       }

      memset(&tempent,0,sizeof(simple_dirent));
      tempent.st_ino = new_inode;
      tempent.st_mode = S_IFDIR | 0755;
      strcpy(tempent.name,&path[tmpptr-path+1]);
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);
      fclose(fptr);
      /*Done with updating the parent inode meta*/


      memset(&inputstat,0,sizeof(struct stat));
      inputstat.st_ino=new_inode;
      inputstat.st_mode = S_IFDIR | 0755;
      inputstat.st_nlink = 2;
      inputstat.st_uid=getuid();
      inputstat.st_gid=getgid();
      sprintf(metapath,"%s/meta%ld",METASTORE,new_inode);
      fptr=fopen(metapath,"w");
      fwrite(&inputstat,sizeof(struct stat),1,fptr);
      num_subdir=2;
      num_reg=0;
      fwrite(&num_subdir,sizeof(long),1,fptr);
      fwrite(&num_reg,sizeof(long),1,fptr);
      memset(&ent1,0,sizeof(simple_dirent));
      memset(&ent2,0,sizeof(simple_dirent));
      ent1.st_ino=new_inode;
      ent1.st_mode=S_IFDIR | 0755;
      strcpy(ent1.name,".");
      ent2.st_ino=this_inode;
      ent2.st_mode=S_IFDIR | 0755;
      strcpy(ent2.name,"..");
      fwrite(&ent1,sizeof(simple_dirent),1,fptr);
      fwrite(&ent2,sizeof(simple_dirent),1,fptr);
      fclose(fptr);
     }
   }

  mysync_system_meta();
  return retcode;
 }


static int myutime(const char *path, struct utimbuf *mymodtime)
 {
  int retcode=0;
  show_current_time();

  return retcode;
 } 

static int myrename(const char *oldname, const char *newname)
 {
  int retcode=0;
  show_current_time();

  return retcode;
 }

static int myunlink(const char *path)
 {
/*Todo: need to revise this to take close after delete into account*/
/*TODO: will need to actually decrease n_link to inode, and to check if
  an opened file points to that inode. */
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  FILE *fptr;
  char metapath[1024];
  char blockpath[1024];
  long num_subdir,num_reg,total_blocks;
  simple_dirent tempent;
  char *tmpptr;
  int tmpstatus;
  int tmp_index,count;
  long block_count;

  show_current_time();

  printf("Debug myunlink: deleting %s\n",path);
  tmpptr = strrchr(path,'/');

  parent_inode = find_parent_inode(path);
  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
    fptr = fopen(metapath,"r");
    if (fptr==NULL)
     return -ENOENT;
    fseek(fptr,sizeof(struct stat), SEEK_SET);
    fread(&total_blocks,sizeof(long),1,fptr);
    fclose(fptr);
    invalidate_inode_cache(path);
    tmpstatus=unlink(metapath);
    if (tmpstatus!=0)
     return -1;
    mysystem_meta.total_inodes -=1;

    /* Removing all blocks for this inode */
    for(block_count=1;block_count<=total_blocks;block_count++)
     {
      sprintf(blockpath,"%s/data_%ld_%ld",BLOCKSTORE,this_inode,block_count);
      unlink(blockpath);
     }

    sprintf(metapath,"%s/meta%ld",METASTORE,parent_inode);

    fptr=fopen(metapath,"r+");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir,sizeof(long),1,fptr);
      fread(&num_reg,sizeof(long),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(num_subdir*sizeof(simple_dirent)),SEEK_SET);
      for(count=0;count<num_reg;count++)
       {
        fread(&tempent,sizeof(simple_dirent),1,fptr);
        printf("To this file %s, check %s\n",tempent.name,&path[(tmpptr-path)+1]);
        if (strcmp(tempent.name,&path[(tmpptr-path)+1])==0)
         {
          tmp_index = count;
          break;
         }
       }
      printf("Testing if have this file\n");
      if (count>=num_reg)
       return -ENOENT;
      num_reg--;
      fseek(fptr,sizeof(struct stat)+sizeof(long),SEEK_SET);
      fwrite(&num_reg,sizeof(long),1,fptr);
      if (tmp_index<num_reg)  /*If the entry to be deleted is not at the end of the meta*/
       {
        fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_reg+num_subdir)*sizeof(simple_dirent)),SEEK_SET);
        fread(&tempent,sizeof(simple_dirent),1,fptr);
        fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((tmp_index+num_subdir)*sizeof(simple_dirent)),SEEK_SET);
        fwrite(&tempent,sizeof(simple_dirent),1,fptr);
       }
      fclose(fptr);
      truncate(metapath,sizeof(struct stat)+(2*sizeof(long))+((num_reg+num_subdir)*sizeof(simple_dirent)));
      printf("finished unlink\n");
     }
   }

   mysync_system_meta();


  return retcode;
 }
static int myrmdir(const char *path)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  FILE *fptr;
  char metapath[1024];
  long num_subdir,num_reg;
  simple_dirent tempent;
  char *tmpptr;
  int tmpstatus;
  int tmp_index,count;

/* TODO: Need to check for if directory empty before proceeding*/

  show_current_time();

  printf("Debug myrmdir: deleting %s\n",path);

  tmpptr = strrchr(path,'/');

  parent_inode = find_parent_inode(path);
  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);

    /*First, check if the subdirectory is empty or not*/

    fptr=fopen(metapath,"r");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir,sizeof(long),1,fptr);
      fread(&num_reg,sizeof(long),1,fptr);
      if ((num_subdir+num_reg)>2)
       return -ENOTEMPTY;
     }



    /*Invalidate the inode cache*/
    invalidate_inode_cache(path);

    tmpstatus=unlink(metapath);
    if (tmpstatus!=0)
     return -1;
    mysystem_meta.total_inodes -=1;

    sprintf(metapath,"%s/meta%ld",METASTORE,parent_inode);

    fptr=fopen(metapath,"r+");
    if (fptr==NULL)
     retcode = -ENOENT;
    else
     {
      fread(&inputstat,sizeof(struct stat),1,fptr);
      inputstat.st_nlink--;
      fseek(fptr,0,SEEK_SET);
      fwrite(&inputstat,sizeof(struct stat),1,fptr);

      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fread(&num_subdir,sizeof(long),1,fptr);
      fread(&num_reg,sizeof(long),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long)),SEEK_SET);
      for(count=0;count<num_subdir;count++)
       {
        fread(&tempent,sizeof(simple_dirent),1,fptr);
        printf("To this directory %s, check %s\n",tempent.name,&path[(tmpptr-path)+1]);
        if (strcmp(tempent.name,&path[(tmpptr-path)+1])==0)
         {
          tmp_index = count;
          break;
         }
       }
      printf("Testing if have this directory\n");
      if (count>=num_subdir)
       return -ENOENT;
      num_subdir--;
      fseek(fptr,sizeof(struct stat),SEEK_SET);
      fwrite(&num_subdir,sizeof(long),1,fptr);
      if (tmp_index<num_subdir)  /*If the entry to be deleted is not at the end of the subdir meta*/
       {
        fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(num_subdir*sizeof(simple_dirent)),SEEK_SET);  /*Last subdir entry*/
        fread(&tempent,sizeof(simple_dirent),1,fptr);
        fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(tmp_index*sizeof(simple_dirent)),SEEK_SET);
        fwrite(&tempent,sizeof(simple_dirent),1,fptr);
       }
      if (num_reg > 0) /*If have reg file, also need to swap*/
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+((num_subdir+num_reg)*sizeof(simple_dirent)),SEEK_SET);  /*Last file entry*/
      fread(&tempent,sizeof(simple_dirent),1,fptr);
      fseek(fptr,sizeof(struct stat)+(2*sizeof(long))+(num_subdir*sizeof(simple_dirent)),SEEK_SET); /*Overwrite the place occupied by the original last subdir entry*/
      fwrite(&tempent,sizeof(simple_dirent),1,fptr);

      fclose(fptr);
      truncate(metapath,sizeof(struct stat)+(2*sizeof(long))+((num_reg+num_subdir)*sizeof(simple_dirent)));
      printf("finished rmdir\n");
     }
   }

  mysync_system_meta();

  return retcode;
 }


static int myfsync(const char *path, int datasync, struct fuse_file_info *fi)
 {
  show_current_time();

  return 0;
 }
static int mytruncate(const char *path, off_t length)
 {
  struct stat inputstat;
  int retcode=0;
  ino_t parent_inode,this_inode;
  FILE *fptr;
  char metapath[1024];
  char blockpath[1024];
  long num_subdir,num_reg,total_blocks,last_block;
  simple_dirent tempent;
  char *tmpptr;
  int tmpstatus;
  int tmp_index,count;
  long block_count;

  show_current_time();

  printf("Debug truncate path %s length %ld\n",path,length);

  this_inode = find_inode(path);

  if (this_inode <=0)
   retcode = -ENOENT;
  else
   {
    sprintf(metapath,"%s/meta%ld",METASTORE,this_inode);
    fptr = fopen(metapath,"r+");
    if (fptr==NULL)
     return -ENOENT;
    fread(&inputstat,sizeof(struct stat),1,fptr);
    fread(&total_blocks,sizeof(long),1,fptr);
    if (length == 0)
     last_block = 0;
    else
     last_block = ((length-1) / (long) MAX_BLOCK_SIZE) + 1;

    printf("Debug truncate: last block is %ld\n",last_block);

    /*First delete blocks that need to be thrown away*/
    for(block_count=last_block+1;block_count<=total_blocks;block_count++)
     {
      printf("Debug truncate: killing block %ld",block_count);
      sprintf(blockpath,"%s/data_%ld_%ld",BLOCKSTORE,this_inode,block_count);
      unlink(blockpath);
     }
    if (length < (last_block * MAX_BLOCK_SIZE))
     {
      sprintf(blockpath,"%s/data_%ld_%ld",BLOCKSTORE,this_inode,last_block);
      truncate(blockpath, length - ((last_block -1) * MAX_BLOCK_SIZE));
     }
    total_blocks = last_block;
    inputstat.st_size=length;
    inputstat.st_blocks = (inputstat.st_size+511)/512;
    fseek(fptr,0,SEEK_SET);
    fwrite(&inputstat,sizeof(struct stat),1,fptr);
    fwrite(&total_blocks,sizeof(long),1,fptr);
    fclose(fptr);
    truncate(metapath,sizeof(struct stat)+sizeof(long)+sizeof(blockent)*last_block);
   }
  return 0;
 }

static struct fuse_operations my_fuse_ops = {
  .getattr = mygetattr,
  .readdir = myreaddir,
  .open = myopen,
  .opendir = myopendir,
  .read = myread,
  .write = mywrite,
  .mknod = mymknod,
  .utime = myutime,
  .rename = myrename,
  .unlink = myunlink,
  .fsync = myfsync,
  .mkdir = mymkdir,
  .rmdir = myrmdir,
  .destroy = mydestroy,
  .truncate = mytruncate,
  .release = myrelease,
 };

void main(int argc, char **argv)
 {

  if (argc < 2)
   {
    printf("Not enough arguments\n");
    return;
   }

  initsystem();
  fuse_main(argc,argv,&my_fuse_ops,NULL);


  return;
 }

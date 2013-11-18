#define FUSE_USE_VERSION 26
#include "fuseop.h"
#include "dir_lookup.h"
#include "super_inode.h"
#include "params.h"
#include <fuse.h>
#include <time.h>

/* TODO: Need to go over the access rights problem for the ops */
/*TODO: Need to invalidate cache entry if rename/deleted */


static int hfuse_getattr(const char *path, struct stat *inode_stat)
 {
  ino_t hit_inode;
  SUPER_INODE_ENTRY tempentry;
  int ret_code;

  hit_inode = lookup_pathname(path);

  if (hit_inode > 0)
   {
    ret_code =super_inode_read(hit_inode, &tempentry);    
    if (ret_code < 0)
     return -ENOENT;
    memcpy(inode_stat,&(tempentry.inode_stat),sizeof(struct stat));
    printf("getattr %ld\n",inode_stat->st_ino);
    return 0;
   }
  else
   return -ENOENT;
 }

//int hfuse_readlink(const char *path, char *buf, size_t buf_size);

static int hfuse_mknod(const char *path, mode_t mode, dev_t dev)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[400];
  ino_t self_inode, parent_inode;
  struct stat this_stat;
  FILE_META_TYPE this_meta;
  mode_t self_mode;
  FILE *metafptr;
  int ret_val;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname);

  free(parentname);
  if (parent_inode < 1)
   return -ENOENT;

  memset(&this_stat,0,sizeof(struct stat));
  memset(&this_meta,0,sizeof(FILE_META_TYPE));

  self_mode = mode | S_IFREG;
  this_stat.st_mode = self_mode;
  this_stat.st_dev = dev;
  this_stat.st_nlink = 1;
  this_stat.st_uid = getuid();
  this_stat.st_gid = getgid();
  this_stat.st_atime = time(NULL);
  this_stat.st_mtime = this_stat.st_atime;
  this_stat.st_ctime = this_stat.st_ctime;

  self_inode = super_inode_new_inode(&this_stat);
  if (self_inode < 1)
   return -EACCES;
  this_stat.st_ino = self_inode;
  memcpy(&(this_meta.thisstat),&this_stat,sizeof(struct stat));

  fetch_meta_path(thismetapath,self_inode);

  metafptr = fopen(thismetapath,"w");

  if (metafptr == NULL)
   return -EACCES;

  ret_val = fwrite(&this_meta,sizeof(FILE_META_TYPE),1,metafptr);
  fclose(metafptr);
  if (ret_val < 1)
   return -EACCES;

  ret_val = dir_add_entry(parent_inode, self_inode, selfname,self_mode);
  if (ret_val < 0)
   return -EACCES;

  return 0;
 }
static int hfuse_mkdir(const char *path, mode_t mode)
 {
  char *parentname;
  char selfname[400];
  char thismetapath[400];
  ino_t self_inode, parent_inode;
  struct stat this_stat;
  DIR_META_TYPE this_meta;
  DIR_ENTRY_PAGE temppage;
  mode_t self_mode;
  FILE *metafptr;
  int ret_val;

  parentname = malloc(strlen(path)*sizeof(char));
  parse_parent_self(path,parentname,selfname);

  parent_inode = lookup_pathname(parentname);

  free(parentname);
  if (parent_inode < 1)
   return -ENOENT;

  memset(&this_stat,0,sizeof(struct stat));
  memset(&this_meta,0,sizeof(DIR_META_TYPE));
  memset(&temppage,0,sizeof(DIR_ENTRY_PAGE));

  self_mode = mode | S_IFDIR;
  this_stat.st_mode = self_mode;
  this_stat.st_nlink = 2;   /*One pointed by the parent, another by self*/
  this_stat.st_uid = getuid();
  this_stat.st_gid = getgid();
  this_stat.st_atime = time(NULL);
  this_stat.st_mtime = this_stat.st_atime;
  this_stat.st_ctime = this_stat.st_ctime;

  self_inode = super_inode_new_inode(&this_stat);
  if (self_inode < 1)
   return -EACCES;
  this_stat.st_ino = self_inode;
  memcpy(&(this_meta.thisstat),&this_stat,sizeof(struct stat));

  fetch_meta_path(thismetapath,self_inode);

  metafptr = fopen(thismetapath,"w");

  if (metafptr == NULL)
   return -EACCES;

  ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);

  if (ret_val < 1)
   {
    fclose(metafptr);
    return -EACCES;
   }
  this_meta.next_subdir_page = ftell(metafptr);
  fseek(metafptr,0,SEEK_SET);

  ret_val = fwrite(&this_meta,sizeof(DIR_META_TYPE),1,metafptr);

  if (ret_val < 1)
   {
    fclose(metafptr);
    return -EACCES;
   }
  temppage.num_entries = 2;
  temppage.dir_entries[0].d_ino = self_inode;
  temppage.dir_entries[1].d_ino = parent_inode;
  strcpy(temppage.dir_entries[0].d_name,".");
  strcpy(temppage.dir_entries[1].d_name,"..");

  ret_val = fwrite(&temppage,sizeof(DIR_ENTRY_PAGE),1,metafptr);
  fclose(metafptr);

  if (ret_val < 1)
   return -EACCES;

  ret_val = dir_add_entry(parent_inode, self_inode, selfname,self_mode);
  if (ret_val < 0)
   return -EACCES;

  return 0;
 }


//int hfuse_unlink(const char *path);
//int hfuse_rmdir(const char *path);
//int hfuse_symlink(const char *oldpath, const char *newpath);


static int hfuse_rename(const char *oldpath, const char *newpath)
 {
  char *parentname1;
  char selfname1[400];
  char *parentname2;
  char selfname2[400];
  ino_t parent_inode1,parent_inode2,self_inode;
  int ret_val;
  SUPER_INODE_ENTRY tempentry;
  mode_t self_mode;

  self_inode = lookup_pathname(oldpath);
  if (self_inode < 1)
   return -ENOENT;

  invalidate_cache_entry(oldpath);

  if (lookup_pathname(newpath) > 0)
   return -EACCES;

  ret_val =super_inode_read(self_inode, &tempentry);    
  if (ret_val < 0)
   return -ENOENT;

  self_mode = tempentry.inode_stat.st_mode;

  /*TODO: Will now only handle simple types (that the target is empty and no symlinks)*/
  parentname1 = malloc(strlen(oldpath)*sizeof(char));
  parentname2 = malloc(strlen(newpath)*sizeof(char));
  parse_parent_self(oldpath,parentname1,selfname1);
  parse_parent_self(newpath,parentname2,selfname2);

  parent_inode1 = lookup_pathname(parentname1);
  parent_inode2 = lookup_pathname(parentname2);

  free(parentname1);
  free(parentname2);
  if ((parent_inode1 < 1) || (parent_inode2 < 1))
   return -ENOENT;
  

  if (parent_inode1 == parent_inode2)
   {
    ret_val = dir_replace_name(parent_inode1, self_inode, selfname1, selfname2, self_mode);
    if (ret_val < 0)
     return -EACCES;
    return 0;
   }

  ret_val = dir_remove_entry(parent_inode1,self_inode,selfname1,self_mode);
  if (ret_val < 0)
   return -EACCES;

  ret_val = dir_add_entry(parent_inode2,self_inode,selfname2,self_mode);

  if (ret_val < 0)
   return -EACCES;


  if ((self_mode & S_IFDIR) && (parent_inode1 != parent_inode2))
   {
    ret_val = change_parent_inode(self_inode, parent_inode1, parent_inode2);
    if (ret_val < 0)
     return -EACCES;
   }
  return 0;
 }

//int hfuse_link(const char *oldpath, const char *newpath);
//int hfuse_chmod(const char *path, mode_t mode);
//int hfuse_chown(const char *path, uid_t owner, gid_t group);
//int hfuse_truncate(const char *path, off_t offset);
int hfuse_open(const char *path, struct fuse_file_info *file_info)
 {
  /*TODO: Need to check permission here*/
  ino_t thisinode;
  long fh;

  thisinode = lookup_pathname(path);
  if (thisinode < 1)
   return -ENOENT;

  fh = open_fh(thisinode);
  if (fh < 0)
   return -ENFILE;

  file_info->fh = fh;

  return 0;
 }

int hfuse_read(const char *path, char *buf, size_t size_org, off_t offset, struct fuse_file_info *file_info)
 {
  FH_ENTRY *fh_ptr;
  long start_block, end_block, current_block;
  long start_page, end_page, current_page;
  long nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  long entry_index;
  long block_index;
  char thisblockpath[400];
  int total_bytes_read;
  int this_bytes_read;
  off_t current_offset;
  int target_bytes_read;
  size_t size;
  char fill_zeros;

  if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
   return 0;

  fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

  flockfile(fh_ptr-> metafptr);
  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);
  funlockfile(fh_ptr-> metafptr);


  if ((fh_ptr->cached_meta).thisstat.st_size < (offset+size_org))
   size = ((fh_ptr->cached_meta).thisstat.st_size - offset);
  else
   size = size_org;

  total_bytes_read = 0;

  start_block = (offset / MAX_BLOCK_SIZE);  /* Block indexing starts at zero */
  end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

  start_page = start_block / MAX_BLOCK_ENTRIES_PER_PAGE; /*Page indexing starts at zero*/
  end_page = end_block / MAX_BLOCK_ENTRIES_PER_PAGE;

  if (fh_ptr->cached_page_index != start_page)
   {
    flockfile(fh_ptr-> metafptr);
    flock(fileno(fh_ptr-> metafptr),LOCK_EX);
    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

    if (fh_ptr->cached_page_index != start_page)  /*Check if other threads have already done the work*/
     seek_page(fh_ptr-> metafptr,fh_ptr, start_page);
    flock(fileno(fh_ptr->metafptr),LOCK_UN);
    funlockfile(fh_ptr->metafptr);
   }

  entry_index = start_block % MAX_BLOCK_ENTRIES_PER_PAGE;

  for(block_index = start_block; block_index <= end_block; block_index++)
   {
    /*TODO: For now, only consider storing locally*/

    sem_wait(&(fh_ptr->block_sem));
    if (fh_ptr->opened_block != block_index)
     {
      if (fh_ptr->opened_block != -1)
       {
        fclose(fh_ptr->blockfptr);
        fh_ptr->opened_block = -1;
       }

      flockfile(fh_ptr->metafptr);
      fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
      fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
      funlockfile(fh_ptr->metafptr);


      if ((fh_ptr->cached_page).block_entries[entry_index].status == ST_NONE)
       {     /*If not stored anywhere, fill with zeros*/
        fill_zeros = TRUE;
       }
      else
        fill_zeros = FALSE;

      if (fill_zeros != TRUE)
       {
        fetch_block_path(thisblockpath,(fh_ptr->cached_meta).thisstat.st_ino,block_index);

        fh_ptr->blockfptr=fopen(thisblockpath,"r+");
        setbuf(fh_ptr->blockfptr,NULL);
        fh_ptr->opened_block = block_index;
       }
     }

    if (fill_zeros != TRUE)
     flock(fileno(fh_ptr->blockfptr),LOCK_SH);

    current_offset = (offset+total_bytes_read) % MAX_BLOCK_SIZE;
    target_bytes_read = MAX_BLOCK_SIZE - current_offset;
    if ((size - total_bytes_read) < target_bytes_read) /*Do not need to read that much*/
     target_bytes_read = size - total_bytes_read;

    if (fill_zeros != TRUE)
     {
      fseek(fh_ptr->blockfptr,current_offset,SEEK_SET);
      this_bytes_read = fread(&buf[total_bytes_read],sizeof(char),target_bytes_read, fh_ptr->blockfptr);
     }
    else
     {
      this_bytes_read = target_bytes_read;
      memset(&buf[total_bytes_read],0,sizeof(char) * target_bytes_read);
     }

    total_bytes_read += this_bytes_read;

    if (fill_zeros != TRUE)
     flock(fileno(fh_ptr->blockfptr),LOCK_UN);
 
    sem_post(&(fh_ptr->block_sem));

    if (this_bytes_read < target_bytes_read) /*Terminate if cannot write as much as we want*/
     break;

    if (block_index < end_block)  /*If this is not the last block, need to advance one more*/
     {
      if ((entry_index+1) >= MAX_BLOCK_ENTRIES_PER_PAGE) /*If may need to change meta, lock*/
       {
        flockfile(fh_ptr-> metafptr);
        flock(fileno(fh_ptr-> metafptr),LOCK_EX);
        advance_block(fh_ptr-> metafptr,fh_ptr,&entry_index);
        flock(fileno(fh_ptr-> metafptr),LOCK_UN);
        funlockfile(fh_ptr-> metafptr);
       }
      else
       entry_index++;
     }
   }

  if (total_bytes_read > 0)
   {
    flockfile(fh_ptr-> metafptr);
    flock(fileno(fh_ptr-> metafptr),LOCK_EX);

    /*Update and flush file meta*/

    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);


    if (total_bytes_read > 0)
     (fh_ptr->cached_meta).thisstat.st_atime = time(NULL);

    fseek(fh_ptr->metafptr,0,SEEK_SET);
    fwrite(&(fh_ptr->cached_meta), sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

    super_inode_update_stat((fh_ptr->cached_meta).thisstat.st_ino, &((fh_ptr->cached_meta).thisstat));

    flock(fileno(fh_ptr-> metafptr),LOCK_UN);
    funlockfile(fh_ptr-> metafptr);
   }

  return total_bytes_read;
 }

int hfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *file_info)
 {
  FH_ENTRY *fh_ptr;
  long start_block, end_block, current_block;
  long start_page, end_page, current_page;
  long nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;
  long entry_index;
  long block_index;
  char thisblockpath[400];
  int total_bytes_written;
  int this_bytes_written;
  off_t current_offset;
  int target_bytes_written;

  if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
   return 0;

  total_bytes_written = 0;

  start_block = (offset / MAX_BLOCK_SIZE);  /* Block indexing starts at zero */
  end_block = ((offset+size-1) / MAX_BLOCK_SIZE);

  start_page = start_block / MAX_BLOCK_ENTRIES_PER_PAGE; /*Page indexing starts at zero*/
  end_page = end_block / MAX_BLOCK_ENTRIES_PER_PAGE;

  fh_ptr = &(system_fh_table.entry_table[file_info->fh]);

  flockfile(fh_ptr-> metafptr);
  flock(fileno(fh_ptr-> metafptr),LOCK_EX);

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);


  if (fh_ptr->cached_page_index != start_page)
   seek_page(fh_ptr-> metafptr,fh_ptr, start_page);

  entry_index = start_block % MAX_BLOCK_ENTRIES_PER_PAGE;

  for(block_index = start_block; block_index <= end_block; block_index++)
   {
    /*TODO: For now, only consider storing locally*/
    /*TODO: Need to be able to modify status to locally stored for cases other than ST_NONE*/

    sem_wait(&(fh_ptr->block_sem));
    if (fh_ptr->opened_block != block_index)
     {
      if (fh_ptr->opened_block != -1)
       {
        fclose(fh_ptr->blockfptr);
        fh_ptr->opened_block = -1;
       }
      fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
      fread(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
      fetch_block_path(thisblockpath,(fh_ptr->cached_meta).thisstat.st_ino,block_index);
      if ((fh_ptr->cached_page).block_entries[entry_index].status == ST_NONE)
       {     /*If not stored anywhere, make it on local disk*/
        fh_ptr->blockfptr=fopen(thisblockpath,"a+");
        fclose(fh_ptr->blockfptr);
        (fh_ptr->cached_page).block_entries[entry_index].status = ST_LDISK;
        fseek(fh_ptr->metafptr, fh_ptr->cached_page_start_fpos,SEEK_SET);
        fwrite(&(fh_ptr->cached_page),sizeof(BLOCK_ENTRY_PAGE),1,fh_ptr->metafptr);
       }

      fh_ptr->blockfptr=fopen(thisblockpath,"r+");
      setbuf(fh_ptr->blockfptr,NULL);
      fh_ptr->opened_block = block_index;
     }
    flock(fileno(fh_ptr->blockfptr),LOCK_EX);

    current_offset = (offset+total_bytes_written) % MAX_BLOCK_SIZE;
    target_bytes_written = MAX_BLOCK_SIZE - current_offset;
    if ((size - total_bytes_written) < target_bytes_written) /*Do not need to write that much*/
     target_bytes_written = size - total_bytes_written;

    fseek(fh_ptr->blockfptr,current_offset,SEEK_SET);
    this_bytes_written = fwrite(&buf[total_bytes_written],sizeof(char),target_bytes_written, fh_ptr->blockfptr);

    total_bytes_written += this_bytes_written;

    flock(fileno(fh_ptr->blockfptr),LOCK_UN);
    sem_post(&(fh_ptr->block_sem));

    if (this_bytes_written < target_bytes_written) /*Terminate if cannot write as much as we want*/
     break;

    if (block_index < end_block)  /*If this is not the last block, need to advance one more*/
     advance_block(fh_ptr-> metafptr,fh_ptr,&entry_index);
   }

  /*Update and flush file meta*/

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fread(&(fh_ptr->cached_meta),sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

  if ((fh_ptr->cached_meta).thisstat.st_size < (offset + total_bytes_written))
   (fh_ptr->cached_meta).thisstat.st_size = (offset + total_bytes_written);

  if (total_bytes_written > 0)
   (fh_ptr->cached_meta).thisstat.st_mtime = time(NULL);

  fseek(fh_ptr->metafptr,0,SEEK_SET);
  fwrite(&(fh_ptr->cached_meta), sizeof(FILE_META_TYPE),1,fh_ptr->metafptr);

  super_inode_update_stat((fh_ptr->cached_meta).thisstat.st_ino, &((fh_ptr->cached_meta).thisstat));

  flock(fileno(fh_ptr-> metafptr),LOCK_UN);
  funlockfile(fh_ptr-> metafptr);

  return total_bytes_written;
 }
  
//int hfuse_statfs(const char *path, struct statvfs *buf);      /*Prototype is linux statvfs call*/
//int hfuse_flush(const char *path, struct fuse_file_info *file_info);
int hfuse_release(const char *path, struct fuse_file_info *file_info)
 {
  ino_t thisinode;

  thisinode = lookup_pathname(path);
  if (file_info->fh < 0)
   return -EBADF;

  if (file_info->fh >= MAX_OPEN_FILE_ENTRIES)
   return -EBADF;

  if (system_fh_table.entry_table_flags[file_info->fh] == FALSE)
   return -EBADF;

  if (system_fh_table.entry_table[file_info->fh].thisinode != thisinode)
   return -EBADF;

  close_fh(file_info->fh);
  return 0;
 }
//int hfuse_fsync(const char *path, int, struct fuse_file_info *file_info);
//int hfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
//int hfuse_getxattr(const char *path, const char *name, char *value, size_t size);
//int hfuse_listxattr(const char *path, char *list, size_t size);
//int hfuse_removexattr(const char *path, const char *name);


static int hfuse_opendir(const char *path, struct fuse_file_info *file_info)
 {
  /*TODO: Need to check for access rights */
  return 0;
 }
static int hfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *file_info)
 {
  ino_t this_inode;
  char pathname[400];
  FILE *fptr;
  int count;
  long thisfile_pos;
  ino_t hit_inode;
  DIR_META_TYPE tempmeta;
  DIR_ENTRY_PAGE temp_page;
  struct stat tempstat;

  fprintf(stderr,"DEBUG readdir entering readdir\n");

  this_inode = lookup_pathname(path);

  if (hit_inode == 0)
   return -ENOENT;

  fetch_meta_path(pathname,this_inode);
  fptr = fopen(pathname,"r");

  fread(&tempmeta,sizeof(DIR_META_TYPE),1,fptr);
  thisfile_pos = tempmeta.next_subdir_page;

  while(thisfile_pos != 0)
   {
    fseek(fptr, thisfile_pos, SEEK_SET);
    fread(&temp_page,sizeof(DIR_ENTRY_PAGE),1,fptr);

    for(count=0;count<temp_page.num_entries;count++)
     {
      tempstat.st_ino = temp_page.dir_entries[count].d_ino;
      tempstat.st_mode = S_IFDIR;
      if (filler(buf,temp_page.dir_entries[count].d_name, &tempstat,0))
       return 0;
     }
    thisfile_pos = temp_page.next_page;
   }

  thisfile_pos = tempmeta.next_file_page;
  while(thisfile_pos != 0)
   {
    fseek(fptr, thisfile_pos, SEEK_SET);
    fread(&temp_page,sizeof(DIR_ENTRY_PAGE),1,fptr);
    for (count=0;count<temp_page.num_entries;count++)
     {
      tempstat.st_ino = temp_page.dir_entries[count].d_ino;
      tempstat.st_mode = S_IFREG;
      if (filler(buf,temp_page.dir_entries[count].d_name, &tempstat,0))
       return 0;
     }
    thisfile_pos = temp_page.next_page;
   }

  fclose(fptr);
  return 0;
 }

int hfuse_releasedir(const char *path, struct fuse_file_info *file_info)
 {
  return 0;
 }
void* hfuse_init(struct fuse_conn_info *conn)
 {
  return ((void*) sys_super_inode);
 }
void hfuse_destroy(void *private_data)
 {
  return;
 }
//int hfuse_create(const char *path, mode_t mode, struct fuse_file_info *file_info);
static int hfuse_access(const char *path, int mode)
 {
  /*TODO: finish this*/
  return 0;
 }


static struct fuse_operations hfuse_ops = {
    .getattr = hfuse_getattr,
    .mknod = hfuse_mknod,
    .mkdir = hfuse_mkdir,
    .readdir = hfuse_readdir,
    .opendir = hfuse_opendir,
    .access = hfuse_access,
    .rename = hfuse_rename,
    .open = hfuse_open,
    .release = hfuse_release,
    .write = hfuse_write,
    .read = hfuse_read,
    .init = hfuse_init,
    .destroy = hfuse_destroy,
    .releasedir = hfuse_releasedir,
 };

int hook_fuse(int argc, char **argv)
 {
  return fuse_main(argc,argv, &hfuse_ops, NULL);
 }

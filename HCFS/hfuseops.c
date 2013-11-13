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
//int hfuse_open(const char *path, struct fuse_file_info *file_info);
//int hfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *file_info);
//int hfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *file_info);
//int hfuse_statfs(const char *path, struct statvfs *buf);      /*Prototype is linux statvfs call*/
//int hfuse_flush(const char *path, struct fuse_file_info *file_info);
//int hfuse_release(const char *path, struct fuse_file_info *file_info);
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
/*put stuff here......*/
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
/*put more stuff here.......*/
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

//int hfuse_releasedir(const char *path, struct fuse_file_info *file_info);
//void* hfuse_init(struct fuse_conn_info *conn);
//void hfuse_destroy(void *private_data);
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
 };

int hook_fuse(int argc, char **argv)
 {
  return fuse_main(argc,argv, &hfuse_ops, NULL);
 }

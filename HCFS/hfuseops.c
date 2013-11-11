#include "fuseop.h"
#include "dir_lookup.h"

int hfuse_getattr(const char *path, struct stat *inode_stat)
 {
  ino_t hit_inode;

  hit_inode = lookup_pathname(path);

  if (hit_inode > 0)
   return super_inode_read(hit_inode, inode_stat);
  else
   return -ENOENT;
 }

//int hfuse_readlink(const char *path, char *buf, size_t buf_size);
//int hfuse_mknod(const char *path, mode_t mode, dev_t dev);
//int hfuse_mkdir(const char *path, mode_t mode);
//int hfuse_unlink(const char *path);
//int hfuse_rmdir(const char *path);
//int hfuse_symlink(const char *oldpath, const char *newpath);
//int hfuse_rename(const char *oldpath, const char *newpath);
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
//int hfuse_opendir(const char *path, struct fuse_file_info *file_info);
int hfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *file_info)
 {
  ino_t this_inode;
  char pathname[400];
  FILE *fptr;
  int count;
  long thisfile_pos;
  ino_t hit_inode;
  FUSE_META_TYPE tempmeta;
  DIR_ENTRY_PAGE temp_page;
  struct stat tempstat;


  this_inode = lookup_pathname(path);

  if (hit_inode == 0)
   return -ENOENT;

  fetch_dirmeta_path(pathname,this_inode);
  fptr = fopen(pathname,"r");

  fread(&tempmeta,sizeof(FUSE_META_TYPE),1,fptr);
  thisfile_pos = tempmeta.next_subdir_page;

  while(thisfile_pos != -1)
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
  while(thisfile_pos != -1)
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
//int hfuse_access(const char *path, int mode);


#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <attr/xattr.h>
#include <semaphore.h>

#define TRUE 1
#define FALSE 0

/*BEGIN META definition*/

#define MAX_DIR_ENTRIES_PER_PAGE 100

typedef struct {
    ino_t d_ino;
    char d_name[256];
  } DIR_ENTRY;

typedef struct {
    struct stat thisstat;
    long next_subdir_page;
    long next_file_page;
  } DIR_META_TYPE;

typedef struct {
    int num_entries;
    DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
    long next_page;
  } DIR_ENTRY_PAGE;

/*END META definition*/

/*BEGIN string utility definition*/
void fetch_meta_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file to pathname*/
void fetch_block_path(char *pathname, ino_t this_inode, long block_num);   /*Will copy the filename of the block file to pathname*/
void parse_parent_self(char *pathname, char *parentname, char *selfname);
/*END string utility definition*/

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode);


/*BEGIN FUSE ops definition*/
int hfuse_getattr(const char *path, struct stat *inode_stat);
int hfuse_readlink(const char *path, char *buf, size_t buf_size);
int hfuse_mknod(const char *path, mode_t mode, dev_t dev);
int hfuse_mkdir(const char *path, mode_t mode);
int hfuse_unlink(const char *path);
int hfuse_rmdir(const char *path);
int hfuse_symlink(const char *oldpath, const char *newpath);
int hfuse_rename(const char *oldpath, const char *newpath);
int hfuse_link(const char *oldpath, const char *newpath);
int hfuse_chmod(const char *path, mode_t mode);
int hfuse_chown(const char *path, uid_t owner, gid_t group);
int hfuse_truncate(const char *path, off_t offset);
int hfuse_open(const char *path, struct fuse_file_info *file_info);
int hfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *file_info);
int hfuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *file_info);
int hfuse_statfs(const char *path, struct statvfs *buf);      /*Prototype is linux statvfs call*/
int hfuse_flush(const char *path, struct fuse_file_info *file_info);
int hfuse_release(const char *path, struct fuse_file_info *file_info);
int hfuse_fsync(const char *path, int, struct fuse_file_info *file_info);
int hfuse_setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
int hfuse_getxattr(const char *path, const char *name, char *value, size_t size);
int hfuse_listxattr(const char *path, char *list, size_t size);
int hfuse_removexattr(const char *path, const char *name);
int hfuse_opendir(const char *path, struct fuse_file_info *file_info);
int hfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *file_info);
int hfuse_releasedir(const char *path, struct fuse_file_info *file_info);
void* hfuse_init(struct fuse_conn_info *conn);
void hfuse_destroy(void *private_data);
int hfuse_create(const char *path, mode_t mode, struct fuse_file_info *file_info);
int hfuse_access(const char *path, int mode);
/*END FUSE ops definition*/
 

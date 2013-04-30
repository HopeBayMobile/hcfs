/* Code under development by Jiahong Wu */

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
#define METASTORE "/storage/home/jiahongwu/myfuse2/metastorage"
#define BLOCKSTORE "/storage/home/jiahongwu/myfuse2/blockstorage"
#define MAX_ICACHE_ENTRY 65536
#define MAX_ICACHE_PATHLEN 1024
#define MAX_FILE_TABLE_SIZE 1024

#define MAX_BLOCK_SIZE 2097152

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

void initsystem();
void mysync_system_meta();
void mydestroy(void *private_data);
void create_root_meta();

void show_current_time();

unsigned int compute_inode_hash(const char *path);
void replace_inode_cache(unsigned int inodehash,const char *fullpath, ino_t st_ino);
ino_t find_inode_fullpath(const char *path, ino_t this_inode, const char *fullpath, unsigned int inodehash);
void invalidate_inode_cache(const char *path);
ino_t find_inode(const char *path);
ino_t find_parent_inode(const char *path);

int mygetattr(const char *path, struct stat *nodestat);
int myreaddir(const char *path, void *buf, fuse_fill_dir_t filler,
    off_t offset, struct fuse_file_info *fi);
int myopen(const char *path, struct fuse_file_info *fi);
int myrelease(const char *path, struct fuse_file_info *fi);
int myopendir(const char *path, struct fuse_file_info *fi);
int myread(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int mywrite(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int mymknod(const char *path, mode_t filemode,dev_t thisdev);
int mymkdir(const char *path,mode_t thismode);
int myutime(const char *path, struct utimbuf *mymodtime);
int myrename(const char *oldname, const char *newname);
int myunlink(const char *path);
int myrmdir(const char *path);
int myfsync(const char *path, int datasync, struct fuse_file_info *fi);
int mytruncate(const char *path, off_t length);
int mystatfs(const char *path, struct statvfs *buf);


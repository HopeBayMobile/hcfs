/* Code under development by Jiahong Wu */

/*TODO:
1. cache replacement
2. restore block from cloud
3. FS recovery
4. FS maintenance
*/

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
#define METASTORE "/storage/home/jiahongwu/mycfs/metastorage"
#define BLOCKSTORE "/storage/home/jiahongwu/mycfs/blockstorage"
#define MAX_ICACHE_ENTRY 65536
#define MAX_ICACHE_PATHLEN 1024
#define MAX_FILE_TABLE_SIZE 1024
#define SYS_DIR_WIDTH 1024
#define MAX_NAME_LEN 256

#define MAX_BLOCK_SIZE 2097152

#define CACHE_SOFT_LIMIT 1024*1024*10  /* Will start actively uploading dirty blocks and swapping out blocks beyond this limit*/
#define CACHE_HARD_LIMIT 1024*1024*20  /* Will stop creation of new cache blocks beyond this limit */

#define True 1
#define False 0

typedef struct {
  ino_t st_ino;
  mode_t st_mode;
  char name[260];
 } simple_dirent;

typedef struct {
  ino_t total_inodes;
  ino_t max_inode;
  ino_t first_free_inode;
  long system_size;
  long cache_size;
 } system_meta;

typedef struct {
  int stored_where;  /*0: not stored, 1: local, 2: cloud, 3: local + cloud, 4: on local but in transit to cloud, 5: on cloud but in transit to local*/
 } blockent;

ino_t total_unclaimed_inode;
FILE *unclaimed_list;

system_meta mysystem_meta;
FILE *system_meta_fptr;
FILE *super_inode_read_fptr, *super_inode_write_fptr;
sem_t *super_inode_read_sem, *super_inode_write_sem, *mysystem_meta_sem, *num_cache_sleep_sem, *check_cache_sem, *check_next_sem;

typedef struct {
  struct stat thisstat;
  ino_t next_free_inode;
  unsigned char is_dirty;
  unsigned char in_transit;
 } super_inode_entry;

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
  sem_t meta_sem;
  sem_t block_sem;
  long opened_block;
  long total_blocks;
  struct stat inputstat;  
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
int myrename(const char *oldpath, const char *newpath);
int myunlink(const char *path);
int myrmdir(const char *path);
int myfsync(const char *path, int datasync, struct fuse_file_info *fi);
int mytruncate(const char *path, off_t length);
int mystatfs(const char *path, struct statvfs *buf);
int mycreate(const char *path, mode_t filemode, struct fuse_file_info *fi);
int mychown(const char *path, uid_t new_uid, gid_t new_gid);
int mychmod(const char *path, mode_t new_mode);


int super_inode_read(struct stat *inputstat,ino_t this_inode);
int super_inode_write(struct stat *inputstat,ino_t this_inode);
int super_inode_create(struct stat *inputstat,ino_t *this_inode);
int super_inode_delete(ino_t this_inode);
int super_inode_reclaim();

int decrease_nlink_ref(struct stat *inputstat);
int dir_remove_filename(ino_t parent_inode, char* filename);
int dir_remove_dirname(ino_t parent_inode, char* dirname);
int dir_add_filename(ino_t this_inode, ino_t new_inode, char *filename);
int dir_add_dirname(ino_t this_inode, ino_t new_inode, char *dirname);

void run_maintenance_loop();
void run_cache_loop();
void sleep_on_cache_full();
void notify_sleep_on_cache();

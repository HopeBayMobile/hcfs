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
#include <sys/mman.h>

#define TRUE 1
#define FALSE 0

/*BEGIN META definition*/

#define MAX_DIR_ENTRIES_PER_PAGE 100
#define MAX_BLOCK_ENTRIES_PER_PAGE 100

#define ST_NONE 0   /* Not stored on any media or storage. Value should be zero.*/
#define ST_LDISK 1  /* Stored only on local cache */
#define ST_CLOUD 2  /* Stored only on cloud storage */
#define ST_BOTH 3   /* Stored both on local cache and cloud storage */
#define ST_LtoC 4   /* In transition from local cache to cloud storage */
#define ST_CtoL 5   /* In transition from cloud storage to local cache */

typedef struct {
    ino_t d_ino;
    char d_name[256];
  } DIR_ENTRY;

typedef struct {
    unsigned char status;
  } BLOCK_ENTRY;

typedef struct {
    struct stat thisstat;
    long total_children;   /*Total children not including "." and "..*/
    long next_subdir_page;
    long next_file_page;
    long next_xattr_page;
  } DIR_META_TYPE;

typedef struct {
    struct stat thisstat;
    long next_block_page;
    long next_xattr_page;
  } FILE_META_TYPE;

typedef struct {
    int num_entries;
    DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
    long next_page;
  } DIR_ENTRY_PAGE;

typedef struct {
    int num_entries;
    BLOCK_ENTRY block_entries[MAX_BLOCK_ENTRIES_PER_PAGE];
    long next_page;
  } BLOCK_ENTRY_PAGE;

/*END META definition*/

/*BEGIN string utility definition*/
void fetch_meta_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file to pathname*/
void fetch_block_path(char *pathname, ino_t this_inode, long block_num);   /*Will copy the filename of the block file to pathname*/
void parse_parent_self(const char *pathname, char *parentname, char *selfname);
/*END string utility definition*/

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode);
int dir_replace_name(ino_t parent_inode, ino_t child_inode, char *oldname, char *newname, mode_t child_mode);
int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode);
int change_parent_inode(ino_t self_inode, ino_t parent_inode1, ino_t parent_inode2);
int decrease_nlink_inode_file(ino_t this_inode);

void init_hfuse();

/*BEGIN definition of file handle */

#define MAX_OPEN_FILE_ENTRIES 65536

typedef struct {
    ino_t thisinode;
    FILE *metafptr;   /*TODO: use flockfile function to lock these ptrs between threads*/
    FILE *blockfptr;
    long opened_block;
    FILE_META_TYPE cached_meta;
//    BLOCK_ENTRY_PAGE cached_page;
    long cached_page_index;
    long cached_page_start_fpos;
    sem_t block_sem;
  } FH_ENTRY;

typedef struct {
    long num_opened_files;
    char *entry_table_flags;
    FH_ENTRY *entry_table;
    long last_available_index;
    sem_t fh_table_sem;
  } FH_TABLE_TYPE;

FH_TABLE_TYPE system_fh_table;

int init_system_fh_table();
long open_fh(ino_t thisinode);
int close_fh(long index);
int seek_page(FILE *fptr, FH_ENTRY *fh_ptr,long target_page);
long advance_block(FILE *fptr, long thisfilepos,long *entry_index); /*In advance block, need to write back dirty page if change page */

/*END definition of file handle */


typedef struct {
    long system_size;
    long dirty_size;
    long cache_size;
    long cache_blocks;
  } SYSTEM_DATA_TYPE;

typedef struct {
    FILE *system_val_fptr;
    SYSTEM_DATA_TYPE systemdata;
    sem_t access_sem;
    sem_t num_cache_sleep_sem;
    sem_t check_cache_sem;
    sem_t check_next_sem;
  } SYSTEM_DATA_HEAD;

SYSTEM_DATA_HEAD *hcfs_system;

FILE *logfptr;

int init_hcfs_system_data();
int sync_hcfs_system_data(char need_lock);
long check_file_size(const char *path);


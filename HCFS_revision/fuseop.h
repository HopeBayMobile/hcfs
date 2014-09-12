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

/*BEGIN META definition*/

#define MAX_DIR_ENTRIES_PER_PAGE 99 /*Max number of children per node is 100, min is 50, so at least 49 elements in each node (except the root) */
#define MAX_BLOCK_ENTRIES_PER_PAGE 100

#define ST_NONE 0   /* Not stored on any media or storage. Value should be zero.*/
#define ST_LDISK 1  /* Stored only on local cache */
#define ST_CLOUD 2  /* Stored only on cloud storage */
#define ST_BOTH 3   /* Stored both on local cache and cloud storage */
#define ST_LtoC 4   /* In transition from local cache to cloud storage */
#define ST_CtoL 5   /* In transition from cloud storage to local cache */
#define ST_TODELETE 6 /* Block to be deleted in backend */

/* TODO: Merge all dir entries to the same page pool, and use b-tree to maintain dir page structure */
#define D_ISDIR 0
#define D_ISREG 1
#define D_ISLNK 2

/* Structures for directories */
typedef struct {
    ino_t d_ino;
    char d_name[256];
    char d_type;
  } DIR_ENTRY;

typedef struct {
    long long total_children;   /*Total children not including "." and "..*/
    long long root_entry_page;
    long long next_xattr_page;
    long long entry_page_gc_list;
  } DIR_META_TYPE;

typedef struct {
    int num_entries;
    DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
    long long this_page_pos; /*File pos of the current node*/
    long long child_page_pos[MAX_DIR_ENTRIES_PER_PAGE+1]; /* File pos of child pages for this node, b-tree style */
    long long parent_page_pos; /*File pos of parent. If this is the root, the value is 0 */
    long long gc_list_next; /*File pos of the next gc entry if on gc list*/
  } DIR_ENTRY_PAGE;

/* Structures for regular files */

typedef struct {
    unsigned char status;
  } BLOCK_ENTRY;

typedef struct {
    long long next_block_page;
    long long next_xattr_page;
  } FILE_META_TYPE;

typedef struct {
    int num_entries;
    BLOCK_ENTRY block_entries[MAX_BLOCK_ENTRIES_PER_PAGE];
    long long next_page;
  } BLOCK_ENTRY_PAGE;

/*END META definition*/

/*BEGIN string utility definition*/
void fetch_meta_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file to pathname*/
void fetch_block_path(char *pathname, ino_t this_inode, long long block_num);   /*Will copy the filename of the block file to pathname*/
void parse_parent_self(const char *pathname, char *parentname, char *selfname);
void fetch_todelete_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file in todelete folder to pathname*/

/*END string utility definition*/

int dir_add_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode);
int dir_remove_entry(ino_t parent_inode, ino_t child_inode, char *childname, mode_t child_mode);
int change_parent_inode(ino_t self_inode, ino_t parent_inode1, ino_t parent_inode2);
int decrease_nlink_inode_file(ino_t this_inode);
int init_dir_page(DIR_ENTRY_PAGE *temppage, ino_t self_inode, ino_t parent_inode, long long this_page_pos);

void init_hfuse();


typedef struct {
    long long system_size;
    long long dirty_size;
    long long cache_size;
    long long cache_blocks;
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


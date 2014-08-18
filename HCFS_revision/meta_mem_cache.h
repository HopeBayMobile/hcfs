/* TODO: A global meta cache and a block data cache in memory. All reads / writes go through the caches, and a parameter controls when to write dirty cache entries back to files (could be write through or after several seconds).*/
/* TODO: Each inode can only occupy at most one meta cache entry (all threads accessing that inode share the same entry). Each data block in each inode can only occupy at most one data cache entry.*/

/* A hard limit defines the upper bound on the number of entries (or mem used?) */
/* Dynamically allocate memory and release memory when not being used for a long time (controlled by a parameter) */

/* Will keep cache entry even after file is closed, until expired or need to be replaced */

/* Cannot open more file if all meta cache entry is occupied by opened files */

/* Data structure

Each meta cache entry keeps
1. Struct stat
2. Struct DIR_META_TYPE or FILE_META_TYPE
3. Up to two dir entry pages cached
4. Up to two block entry pages cached
5. Up to two xattr pages cached
6. Number of opened handles to the inode
7. Semaphore to the entry
8. Last access time
9. Dirty or clean status for items 1 to 5

Lookup of cache entry:

inode => hashtable lookup (hash table with doubly linked list) => return pointer to the entry
Each lookup entry contains the doubly linked list structure, plus number of opened handles to the inode and the inode number. Finally, the pointer to the actual
cache entry is stored.


Each entry in the hashtable (the header of each linked list) contains a semaphore to this list. If two accesses collide at the same time, one
has to wait for the completion of the other. This is to ensure the atomic completion of adding and deleting cache entries.
If deleting cache entry, will need to acquire both the header lock and the entry lock before proceeding. If cache entry lock cannot be acquired immediately,
should release header lock and sleep for a short time, or skip to other entries.


*/

typedef struct {
  struct stat this_stat;
  char stat_dirty;
  mode_t inode_mode;
  DIR_META_TYPE dir_meta;    /* Only used if inode is a dir */
  FILE_META_TYPE file_meta;  /* Only used if inode is a reg file */
  char meta_dirty;
  DIR_ENTRY_PAGE (*dir_entry_cache)[2];      /*Zero if not pointed to any page*/
  long dir_entry_cache_pos[2];               /* TODO: How to flush cached pages due to cache full: index 0 means newer entry, index 1 means older. Always first flush index 1, copy index 0 to 1, then put new page to index 0 */
  char dir_entry_cache_dirty[2];
  BLOCK_ENTRY_PAGE (*block_entry_cache)[2];
  long block_entry_cache_pos[2];
  char block_entry_cache_dirty[2];
/* TODO: Add xattr page cached here */
  int opened_handles_to_inode;
  sem_t access_sem;
  struct timeval last_access_time;  /*TODO: Need to think whether system clock change could affect the involved operations*/
 } META_CACHE_ENTRY_STRUCT;

struct meta_cache_lookup_struct {
  META_CACHE_ENTRY_STRUCT cache_entry_body;
  ino_t inode_num;
  int opened_handles_to_inode;
  char something_dirty;
  struct meta_cache_lookup_struct *next;
  struct meta_cache_lookup_struct *prev;
 };

typedef struct meta_cache_lookup_struct META_CACHE_LOOKUP_ENTRY_STRUCT;

typedef struct {
  META_CACHE_LOOKUP_ENTRY_STRUCT *meta_cache_entries;
  sem_t header_sem;
  int num_entries;
 } META_CACHE_HEADER_STRUCT;

int init_meta_cache_headers();
int release_meta_cache_headers();

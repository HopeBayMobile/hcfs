#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    struct stat inode_stat;
    ino_t util_ll_next;
    ino_t util_ll_prev;
    char status;      /* status is one of NO_LL, IS_DIRTY, TO_BE_DELETED, TO_BE_RECLAIMED, or RECLAIMED*/
    char in_transit;  /*Indicate that the data in this inode is being synced to cloud, but not finished*/
    char mod_after_in_transit;
    ino_t this_index;
  } SUPER_INODE_ENTRY;

typedef struct {
    long num_inode_reclaimed;
    ino_t first_reclaimed_inode;
    ino_t last_reclaimed_inode;
    ino_t first_dirty_inode;
    ino_t last_dirty_inode;
    ino_t first_to_delete_inode;
    ino_t last_to_delete_inode;
    ino_t first_block_cached_inode;
    ino_t last_block_cached_inode;

    long num_to_be_reclaimed;
    long num_to_be_deleted;
    long num_dirty;
    long num_block_cached;

    long num_total_inodes;    /*This defines the total number of inode numbers allocated, including in use and deleted but to be reclaimed*/
    long num_active_inodes;   /*This defines the number of inodes that are currently being used*/
    long total_system_size;
  } SUPER_INODE_HEAD;

typedef struct {
    SUPER_INODE_HEAD head;
    FILE *iofptr;
    FILE *unclaimed_list_fptr;
    sem_t io_sem;
  } SUPER_INODE_CONTROL;

SUPER_INODE_CONTROL *sys_super_inode;

int super_inode_init();   /*Will need to put super_inode_control in a shared memory area*/
int super_inode_destroy();
int super_inode_read(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr);
int super_inode_write(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr);
int super_inode_to_delete(ino_t this_inode);
int super_inode_delete(ino_t this_inode);
int super_inode_reclaim();  
int super_inode_reclaim_fullscan(); /*Conducting full reclaim scan*/
ino_t super_inode_new_inode(struct stat *in_stat);
int super_inode_update_stat(ino_t this_inode, struct stat *newstat);

int ll_enqueue(ino_t thisinode, char which_ll, SUPER_INODE_ENTRY *this_entry);
int ll_dequeue(ino_t thisinode, SUPER_INODE_ENTRY *this_entry);
int write_super_inode_head();
int read_super_inode_entry(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr);
int write_super_inode_entry(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr);

int super_inode_update_transit(ino_t this_inode, char is_start_transit);
int super_inode_mark_dirty(ino_t this_inode);


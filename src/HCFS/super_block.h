typedef struct {
    struct stat inode_stat;
    ino_t util_ll_next;
    ino_t util_ll_prev;
    char status;      /* status is one of NO_LL, IS_DIRTY, TO_BE_DELETED, TO_BE_RECLAIMED, or RECLAIMED*/
    char in_transit;  /*Indicate that the data in this inode is being synced to cloud, but not finished*/
    char mod_after_in_transit;
    ino_t this_index;
  } SUPER_BLOCK_ENTRY;

typedef struct {
    long long num_inode_reclaimed;
    ino_t first_reclaimed_inode;
    ino_t last_reclaimed_inode;
    ino_t first_dirty_inode;
    ino_t last_dirty_inode;
    ino_t first_to_delete_inode;
    ino_t last_to_delete_inode;

    long long num_to_be_reclaimed;
    long long num_to_be_deleted;
    long long num_dirty;
    long long num_block_cached;

    long long num_total_inodes;    /*This defines the total number of inode numbers allocated, including in use and deleted but to be reclaimed*/
    long long num_active_inodes;   /*This defines the number of inodes that are currently being used*/
    long long total_system_size;
  } SUPER_BLOCK_HEAD;

typedef struct {
    SUPER_BLOCK_HEAD head;
    int iofptr;
    FILE *unclaimed_list_fptr;
    sem_t exclusive_lock_sem;
    sem_t share_lock_sem;
    sem_t share_CR_lock_sem;
    int share_counter;
  } SUPER_BLOCK_CONTROL;

SUPER_BLOCK_CONTROL *sys_super_block;

int super_block_init();   /*Will need to put super_block_control in a shared memory area*/
int super_block_destroy();
int super_block_read(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int super_block_write(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int super_block_to_delete(ino_t this_inode);
int super_block_delete(ino_t this_inode);
int super_block_reclaim();  
int super_block_reclaim_fullscan(); /*Conducting full reclaim scan*/
ino_t super_block_new_inode(struct stat *in_stat);
int super_block_update_stat(ino_t this_inode, struct stat *newstat);

int ll_enqueue(ino_t thisinode, char which_ll, SUPER_BLOCK_ENTRY *this_entry);
int ll_dequeue(ino_t thisinode, SUPER_BLOCK_ENTRY *this_entry);
int write_super_block_head();
int read_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);
int write_super_block_entry(ino_t this_inode, SUPER_BLOCK_ENTRY *inode_ptr);

int super_block_update_transit(ino_t this_inode, char is_start_transit);
int super_block_mark_dirty(ino_t this_inode);
int super_block_share_locking();
int super_block_share_release();
int super_block_exclusive_locking();
int super_block_exclusive_release();

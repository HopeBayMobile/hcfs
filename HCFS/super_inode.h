#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    struct stat inode_stat;
    char is_dirty;
    char to_be_deleted;   /*Meta and local blocks are deleted if this is true, but backend objects are still being deleted in background. Should not reclaim.*/
    char to_be_reclaimed; /*Meta, local blocks, and backend objects are deleted completely. Can be reclaimed.*/
    ino_t next_reclaimed_inode;
    ino_t this_index;
  } SUPER_INODE_ENTRY;

typedef struct {
    long num_inode_reclaimed;
    ino_t first_reclaimed_inode;
    ino_t last_reclaimed_inode;
    long num_to_be_reclaimed;
    long num_total_inodes;    /*This defines the total number of inode numbers allocated, including in use and deleted but to be reclaimed*/
    long num_active_inodes;   /*This defines the number of inodes that are currently being used*/
  } SUPER_INODE_HEAD;

typedef struct {
    SUPER_INODE_HEAD head;
    FILE *iofptr;
    sem_t io_sem;
  } SUPER_INODE_CONTROL;

SUPER_INODE_CONTROL *sys_super_inode;

int super_inode_init();   /*Will need to put super_inode_control in a shared memory area*/
int super_inode_destroy();
int super_inode_read(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr);
int super_inode_write(ino_t this_inode, SUPER_INODE_ENTRY *inode_ptr);
int super_inode_to_delete(ino_t this_inode);
int super_inode_delete(ino_t this_inode);
int super_inode_reclaim(int fullscan);  /*fullscan is a reserved flag for conducting full reclaim scan*/
ino_t super_inode_new_inode(struct stat *in_stat);
int super_inode_update_stat(ino_t this_inode, struct stat *newstat);

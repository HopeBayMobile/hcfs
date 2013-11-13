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
    struct stat thisstat;
    long total_blocks;
    long next_block_page;
    long next_xattr_page;
  } FILE_META_TYPE;

typedef struct {
    int num_entries;
    DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
    long next_page;
  } DIR_ENTRY_PAGE;

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

void init_hfuse();
 

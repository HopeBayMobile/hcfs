#include "fuseop.h"

char NOW_NO_ROOTS;
char NOW_TEST_RESTORE_META;
char RESTORED_META_NOT_FOUND;
char NO_PARENTS; 
char record_inode[100000];
sem_t record_inode_sem;
ino_t max_record_inode;

struct stat exp_stat;
FILE_META_TYPE exp_filemeta;

char *remove_list[100];
int32_t remove_count;

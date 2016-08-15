#include "fuseop.h"

char NOW_NO_ROOTS;
char NOW_TEST_RESTORE_META;
char record_inode[100000];
sem_t record_inode_sem;
ino_t max_record_inode;

struct stat exp_stat;
FILE_META_TYPE exp_filemeta;

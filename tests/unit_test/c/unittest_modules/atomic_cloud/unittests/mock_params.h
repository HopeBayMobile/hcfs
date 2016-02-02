#include <semaphore.h>

typedef struct {
	sem_t record_sem;
	ino_t record_uploading_inode[500];
	int total_inode;
} TEST_REVERT_STRUCT;

TEST_REVERT_STRUCT test_sync_struct;
TEST_REVERT_STRUCT test_delete_struct;

char is_first_upload;
char fetch_from_cloud_fail;

#define HTTP_OK 200
#define HTTP_FAIL 500

/* Define for hcfs_fromcloud */
#define BLOCK_NUM__FETCH_SUCCESS 1
#define BLOCK_NUM__FETCH_FAIL 2

#define EXTEND_FILE_SIZE 1234

/* Define for hcfs_clouddelete */
#define INODE__FETCH_TODELETE_PATH_SUCCESS 1
#define INODE__FETCH_TODELETE_PATH_FAIL 2

#define TODELETE_PATH "/tmp/todelete_meta_path"
#include "params.h"
#include <semaphore.h>
char **objname_list;
int objname_counter;
sem_t objname_counter_sem;

char upload_ctl_todelete_blockno[100];
SYSTEM_CONF_STRUCT system_config;

typedef struct {
	int *to_handle_inode; // inode_t to be deleted by delete_loop()
	int tohandle_counter; // counter used by super_block
	int num_inode; // total number of to_delete_inode
} LoopTestData;

typedef struct{
	int *record_handle_inode; // Recorded in inode array when inode is called
	int record_inode_counter; 
	sem_t record_inode_sem;
} LoopToVerifiedData;

LoopTestData test_data;
LoopToVerifiedData to_verified_data;

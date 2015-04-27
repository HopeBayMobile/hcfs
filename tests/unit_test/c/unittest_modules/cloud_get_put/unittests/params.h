#define HTTP_OK 200
#define HTTP_FAIL 500

/* Define for hcfs_fromcloud */
#define BLOCK_NO__FETCH_SUCCESS 1
#define BLOCK_NO__FETCH_FAIL 2

#define EXTEND_FILE_SIZE 1234

/* Define for hcfs_clouddelete */
#define INODE__FETCH_TODELETE_PATH_SUCCESS 1
#define INODE__FETCH_TODELETE_PATH_FAIL 2

#define TODELETE_PATH "/tmp/todelete_meta_path"


#include <semaphore.h>
char **delete_objname;
int objname_counter;
sem_t objname_counter_sem;

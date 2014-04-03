#include <curl/curl.h>
#include <semaphore.h>
#include <pthread.h>

#define MAX_UPLOAD_CONCURRENCY 16
#define MAX_SYNC_CONCURRENCY 16

typedef struct {
    off_t page_filepos;
    long long page_entry_index;
    ino_t inode;
    long long blockno;
    char is_block;
    char is_delete;
    int which_curl;
    char tempfilename[400];
 } UPLOAD_THREAD_TYPE;

typedef struct {
    ino_t inode;
    mode_t this_mode;
 } SYNC_THREAD_TYPE;

typedef struct {
    sem_t upload_queue_sem; /*Initialize this to MAX_UPLOAD_CONCURRENCY. Decrease when created a thread for upload, and increase when a thread finished and is joined by the handler thread*/
    sem_t upload_op_sem;
    pthread_t upload_handler_thread;
    UPLOAD_THREAD_TYPE upload_threads[MAX_UPLOAD_CONCURRENCY];
    pthread_t upload_threads_no[MAX_UPLOAD_CONCURRENCY];
    char upload_threads_in_use[MAX_UPLOAD_CONCURRENCY];
    char upload_threads_created[MAX_UPLOAD_CONCURRENCY];
    int total_active_upload_threads;
/*upload threads: used for upload objects to backends*/

  } UPLOAD_THREAD_CONTROL;

typedef struct {
    sem_t sync_op_sem;
    sem_t sync_queue_sem; /*similar to upload_queue_sem*/
    pthread_t sync_handler_thread;
    pthread_t inode_sync_thread[MAX_SYNC_CONCURRENCY];
    ino_t sync_threads_in_use[MAX_SYNC_CONCURRENCY];
    char sync_threads_created[MAX_SYNC_CONCURRENCY];
    int total_active_sync_threads;
/*sync threads: used for syncing meta/block in a single inode*/
  } SYNC_THREAD_CONTROL;

UPLOAD_THREAD_CONTROL upload_thread_control;
SYNC_THREAD_CONTROL sync_thread_control;

void init_upload_control();
void init_sync_control();
void sync_single_inode(SYNC_THREAD_TYPE *ptr);
void collect_finished_sync_threads(void *ptr);
void collect_finished_upload_threads(void *ptr);
void dispatch_upload_block(int which_curl);
void dispatch_delete_block(int which_curl);
void schedule_sync_meta(FILE *metafptr,int which_curl);
void con_object_sync(UPLOAD_THREAD_TYPE *upload_thread_ptr);
void delete_object_sync(UPLOAD_THREAD_TYPE *upload_thread_ptr);
void upload_loop();


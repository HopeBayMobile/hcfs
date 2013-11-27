#include "myfuse.h"
#include "mycurl.h"

#define obj_page_size 6400
#define META 1
#define BLOCK 2
#define META_BLOCK 3

/* Definition: Head is the earliest to-delete entry. Tail is the latest one.
Any new entry will be appended to the tail. Any deletion will be popped from head.*/
/*TODO: when deleting objects, if object type is meta, will need to call super_inode_delete, either here or datasync.c */
/*TODO: use a pipe to communicate between FUSE process and datasync process */
/*TODO: In datasync process, will need to first fork a thread to handle reading of the deletion pipe and put it in the to-delete pages*/
/*TODO: There should be one deletion pipe write semaphre in FUSE process and one to-delete page access semaphore in the datasync process*/

typedef struct {
    char obj_type;
    ino_t inode_num;
    long block_num_start;
    long block_num_end;
  } object_entry_type;

struct obj_page_template {
    object_entry_type object_entry[obj_page_size];
    int page_head;
    int page_tail;
    struct obj_page_template *next_page;
  };

typedef struct obj_page_template obj_page_type;

obj_page_type *delete_object_head_page = NULL;
obj_page_type *delete_object_tail_page = NULL;

void cleanup_delete_object_list()
 {
  obj_page_type *page_ptr;

  page_ptr = delete_object_head_page;

  while(page_ptr != NULL)
   {
    delete_object_head_page = delete_object_head_page->next_page;
    free(page_ptr);
    page_ptr = delete_object_head_page;
   }
  delete_object_head_page = NULL;
  return;
 }

void init_delete_object_list()
 {
  cleanup_delete_object_list();

  delete_object_head_page = malloc(sizeof(obj_page_type));
  memset(delete_object_head_page,0,sizeof(obj_page_type));

  delete_object_head_page -> next_page = NULL;
  delete_object_tail_page = delete_object_head_page;
  return;
 }

void enqueue_delete_object(char obj_type,ino_t inode_num, long block_num_start, long block_num_end)
 {
  object_entry_type temp_entry;

  temp_entry.obj_type = obj_type;
  temp_entry.inode_num = inode_num;
  temp_entry.block_num_start = block_num_start;
  temp_entry.block_num_end = block_num_end;

  sem_wait(&delete_enqueue_sem);
  write(delete_pipe[1], &temp_entry, sizeof(object_entry_type));
  sem_post(&delete_enqueue_sem);
  return;
 }

void dequeue_delete_object()

void save_delete_objects()

void load_delete_objects()

void object_deletion_loop()
 {
  /*TODO: Will need multiple curl handles here also*/
  /*TODO: This will run in a separate process or thread?*/
  /* TODO: Perhaps this belongs in datasync.c, and use the curl handles there?*/
 }

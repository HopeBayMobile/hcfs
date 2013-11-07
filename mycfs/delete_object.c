#define obj_page_size 6400
#define META 1
#define BLOCK 2

/* Definition: Head is the earliest to-delete entry. Tail is the latest one.
Any new entry will be appended to the tail. Any deletion will be popped from head.*/
/*TODO: when deleting objects, if object type is meta, will need to call super_inode_delete, either here or datasync.c */

typedef struct {
    char obj_type;
    ino_t inode_num;
    long block_num;
  } object_entry_type;

struct obj_page_template {
    object_entry_type object_entry[obj_page_size];
    int page_head;
    int page_tail;
    struct obj_page_template *next_page;
  };

typedef struct obj_page_template obj_page_type;

obj_page_type delete_object_head_page = NULL;
obj_page_type delete_object_tail_page = NULL;

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

void object_deletion_loop()
 {
  /*TODO: Will need multiple curl handles here also*/
  /*TODO: This will run in a separate process or thread?*/
 }

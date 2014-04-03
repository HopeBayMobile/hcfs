#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

#define CACHE_USAGE_NUM_ENTRIES 65536

typedef struct usage_node_template {
    ino_t this_inode;
    long long clean_cache_size;
    long long dirty_cache_size;
    time_t last_access_time;
    time_t last_mod_time;
    struct usage_node_template *next_node;
  } CACHE_USAGE_NODE;

CACHE_USAGE_NODE* inode_cache_usage_hash[CACHE_USAGE_NUM_ENTRIES];
int nonempty_cache_hash_entries;

void sleep_on_cache_full();
void notify_sleep_on_cache();
void run_cache_loop();

void cache_usage_hash_init();
CACHE_USAGE_NODE* return_cache_usage_node(ino_t this_inode); /*Pops the entry from the linked list if found, else NULL is returned*/
void insert_cache_usage_node(ino_t this_inode, CACHE_USAGE_NODE *this_node);
int compare_cache_usage(CACHE_USAGE_NODE *first_node, CACHE_USAGE_NODE *second_node);
/*For compare_cache_usage, returns a negative number (< 0) if the first node needs to be placed in front of the second node, returns zero if does not matter, returns a positive number (> 0) if the second node needs to be placed in front of the first node*/
void build_cache_usage();

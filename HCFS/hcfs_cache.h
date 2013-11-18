#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

typedef struct {
    sem_t num_cache_sleep_sem;
    sem_t check_cache_sem;
    sem_t check_next_sem;
    sem_t cache_update_sem;
    long cached_size;
    long cached_blocks;
  } CACHE_CONTROL_TYPE;



void sleep_on_cache_full();
void notify_sleep_on_cache();

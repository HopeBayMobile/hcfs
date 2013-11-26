#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

void sleep_on_cache_full();
void notify_sleep_on_cache();
void run_cache_loop();


#define MAX_PATHNAME 256
#define PATHNAME_CACHE_ENTRY_NUM 65536
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <semaphore.h>

typedef struct {
    char pathname[MAX_PATHNAME+10];
    ino_t inode_number;
    sem_t cache_entry_sem;
  } PATHNAME_CACHE_ENTRY;

PATHNAME_CACHE_ENTRY pathname_cache[PATHNAME_CACHE_ENTRY_NUM];

ino_t lookup_pathname(const char *path, int *errcode);
ino_t lookup_pathname_recursive(ino_t subroot, int prepath_length, const char *partialpath, const char *fullpath, int *errcode);
unsigned long long compute_hash(const char *path);
void init_pathname_cache();
void replace_pathname_cache(long long index, char *path, ino_t inode_number);
void invalidate_cache_entry(const char *path);  /* If a dir or file is removed or changed, by e.g. rename, move, rm, rmdir, this function has to be called */
ino_t check_cached_path(const char *path);

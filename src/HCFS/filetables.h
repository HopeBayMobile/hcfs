/*BEGIN definition of file handle */

#define MAX_OPEN_FILE_ENTRIES 65536

typedef struct {
    ino_t thisinode;
    META_CACHE_ENTRY_STRUCT *meta_cache_ptr;
    char meta_cache_locked;
    FILE *blockfptr;
    long long opened_block;
    long long cached_page_index;
    long long cached_filepos;
//    BLOCK_ENTRY_PAGE cached_page;
    sem_t block_sem;
  } FH_ENTRY;

typedef struct {
    long long num_opened_files;
    char *entry_table_flags;
    FH_ENTRY *entry_table;
    long long last_available_index;
    sem_t fh_table_sem;
  } FH_TABLE_TYPE;

FH_TABLE_TYPE system_fh_table;

int init_system_fh_table();
long long open_fh(ino_t thisinode);
int close_fh(long long index);
int seek_page(FH_ENTRY *fh_ptr,long long target_page);
long long advance_block(META_CACHE_ENTRY_STRUCT *body_ptr, off_t thisfilepos,long long *entry_index); /*In advance block, need to write back dirty page if change page */

/*END definition of file handle */



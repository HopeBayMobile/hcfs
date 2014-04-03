#define METAPATH "/storage/HCFS/metastorage"
#define BLOCKPATH "/storage/HCFS/blockstorage"
#define SUPERINODE "/storage/HCFS/metastorage/superinode"
#define UNCLAIMEDFILE "/storage/HCFS/metastorage/unclaimedlist"
#define HCFSSYSTEM "/storage/HCFS/metastorage/hcfssystemfile"

#define NUMSUBDIR 1000

#define RECLAIM_TRIGGER 10000

#define CACHE_SOFT_LIMIT ((long long)1024*1024*512)  /*512MB soft cache limit*/
#define CACHE_HARD_LIMIT ((long long)1024*1024*1024)
#define CACHE_DELTA 10485760

#define MAX_BLOCK_SIZE 131072

#define NO_LL 0
#define IS_DIRTY 1
#define TO_BE_DELETED 2
#define TO_BE_RECLAIMED 3
#define RECLAIMED 4

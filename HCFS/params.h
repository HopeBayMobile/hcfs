#define METAPATH "/storage/home/jiahongwu/HCFS/metastorage"
#define BLOCKPATH "/storage/home/jiahongwu/HCFS/blockstorage"
#define SUPERINODE "/storage/home/jiahongwu/HCFS/metastorage/superinode"
#define UNCLAIMEDFILE "/storage/home/jiahongwu/HCFS/metastorage/unclaimedlist"
#define HCFSSYSTEM "/storage/home/jiahongwu/HCFS/metastorage/hcfssystemfile"

#define NUMSUBDIR 1000

#define RECLAIM_TRIGGER 10000

#define CACHE_SOFT_LIMIT (1024*1024*200)  /*200MB soft cache limit*/
#define CACHE_HARD_LIMIT (1024*1024*300)

#define MAX_BLOCK_SIZE 2097152

#define NO_LL 0
#define IS_DIRTY 1
#define TO_BE_DELETED 2
#define TO_BE_RECLAIMED 3
#define RECLAIMED 4
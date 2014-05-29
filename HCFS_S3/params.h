#define METAPATH "/home/jiahongwu/HCFS_S3/metastorage"
#define BLOCKPATH "/home/jiahongwu/HCFS_S3/blockstorage"
#define SUPERINODE "/home/jiahongwu/HCFS_S3/metastorage/superinode"
#define UNCLAIMEDFILE "/home/jiahongwu/HCFS_S3/metastorage/unclaimedlist"
#define HCFSSYSTEM "/home/jiahongwu/HCFS_S3/metastorage/hcfssystemfile"

#define NUMSUBDIR 1000

#define RECLAIM_TRIGGER 10000

#define CACHE_SOFT_LIMIT ((long long)1024*1024*512)  /*512MB soft cache limit*/
#define CACHE_HARD_LIMIT ((long long)1024*1024*1024)
#define CACHE_DELTA 10485760

#define MAX_BLOCK_SIZE 1048576

#define NO_LL 0
#define IS_DIRTY 1
#define TO_BE_DELETED 2
#define TO_BE_RECLAIMED 3
#define RECLAIMED 4

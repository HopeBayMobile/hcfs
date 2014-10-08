Repository for Gateway 2.0 HCFS
===============================

Latest version under HCFS_revision/


Config file
-----------

Please put the following as a text file under /etc/hcfs.conf

METAPATH = <Directory of meta file storage, must exist>
BLOCKPATH = <Diectory of block file storage, must exist>
SUPERINODE = <Path for super inode, suggested under METAPATH>
UNCLAIMEDFILE = <Path for the list of unclaimed inodes, suggested under METAPATH>
HCFSSYSTEM = <Path for the system file, suggested under METAPATH>
CACHE_SOFT_LIMIT = <Soft limit for triggering cache replacement, in bytes>
CACHE_HARD_LIMIT = <Hard limit for blocking further IO, in bytes>
CACHE_DELTA = <Amount of cache to be replaced after hitting hard limit and before allowing further IO, in bytes>
MAX_BLOCK_SIZE = <Data block size, in bytes>


Example:

METAPATH= /home/jiahongwu/testHCFS/metastorage
BLOCKPATH = /home/jiahongwu/testHCFS/blockstorage
SUPERINODE = /home/jiahongwu/testHCFS/metastorage/superinode
UNCLAIMEDFILE= /home/jiahongwu/testHCFS/metastorage/unclaimedlist
HCFSSYSTEM= /home/jiahongwu/testHCFS/metastorage/hcfssystemfile  
CACHE_SOFT_LIMIT = 53687091
CACHE_HARD_LIMIT = 107374182 
CACHE_DELTA = 10485760
MAX_BLOCK_SIZE = 1048576



Backend Configuration
---------------------

Edit the following under hcfscurl.h, and recompile hcfs.
TODO: Move the configuration to the config file as well.

<Replace "SWIFT" with "S3" in the following if S3 backend is used>
#define CURRENT_BACKEND SWIFT

<For swift backend>
#define MY_ACCOUNT 
#define MY_USER 
#define MY_PASS 
#define MY_URL 

<For S3 backend>
#define S3_ACCESS 
#define S3_SECRET 
#define S3_URL 
#define S3_BUCKET 
#define S3_BUCKET_URL 
 
Please use different bucket for each HCFS, as prefix in bucket is not implemented now.


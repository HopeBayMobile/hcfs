/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: params.h
* Abstract: The header file for handling system parameters for HCFS
*
* Revision History
* 2015/2/11 Jiahong added header for this file and revised coding style.
*
**************************************************************************/

#ifndef GW20_SRC_PARAMS_H_
#define GW20_SRC_PARAMS_H_

typedef struct {
	char *metapath;
	char *blockpath;
	char *superblock_name;
	char *unclaimed_name;
	char *hcfssystem_name;
	long long cache_soft_limit;
	long long cache_hard_limit;
	long long cache_update_delta;
	long long max_block_size;
} SYSTEM_CONF_STRUCT;

#define METAPATH system_config.metapath
#define BLOCKPATH system_config.blockpath
#define SUPERBLOCK system_config.superblock_name
#define UNCLAIMEDFILE system_config.unclaimed_name
#define HCFSSYSTEM system_config.hcfssystem_name

#define CACHE_SOFT_LIMIT system_config.cache_soft_limit
#define CACHE_HARD_LIMIT system_config.cache_hard_limit
#define CACHE_DELTA system_config.cache_update_delta

#define MAX_BLOCK_SIZE system_config.max_block_size

#define MAX_META_MEM_CACHE_ENTRIES 5000
#define NUM_META_MEM_CACHE_HEADERS 5000
#define META_CACHE_FLUSH_NOW TRUE

#define METAPATHLEN 400
#define MAX_FILE_NAME_LEN 400

#define NUMSUBDIR 1000

#define RECLAIM_TRIGGER 10000

#define NO_LL 0
#define IS_DIRTY 1
#define TO_BE_DELETED 2
#define TO_BE_RECLAIMED 3
#define RECLAIMED 4

#define DEFAULT_CONFIG_PATH "/etc/hcfs.conf"

#endif  /* GW20_SRC_PARAMS_H_ */

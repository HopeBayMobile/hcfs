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
	int log_level;
	char *log_path;
	char *metapath;
	char *blockpath;
	char *superblock_name;
	char *unclaimed_name;
	char *hcfssystem_name;
	long long cache_soft_limit;
	long long cache_hard_limit;
	long long cache_update_delta;
	long long max_block_size;
	int current_backend;
	char *swift_account;
	char *swift_user;
	char *swift_pass;
	char *swift_url;
	char *swift_container;
	char *swift_protocol;
	char *s3_access;
	char *s3_secret;
	char *s3_url;
	char *s3_bucket;
	char *s3_protocol;
	char *s3_bucket_url;
} SYSTEM_CONF_STRUCT;

#define LOG_LEVEL system_config.log_level
#define LOG_PATH system_config.log_path

#define S3 2
#define SWIFT 1
#define NONE 0

#define CURRENT_BACKEND system_config.current_backend

#define SWIFT_ACCOUNT system_config.swift_account
#define SWIFT_USER system_config.swift_user
#define SWIFT_PASS system_config.swift_pass
#define SWIFT_URL system_config.swift_url
#define SWIFT_CONTAINER system_config.swift_container
#define SWIFT_PROTOCOL system_config.swift_protocol

#define S3_ACCESS system_config.s3_access
#define S3_SECRET system_config.s3_secret
#define S3_URL system_config.s3_url
#define S3_BUCKET system_config.s3_bucket
#define S3_PROTOCOL system_config.s3_protocol
#define S3_BUCKET_URL system_config.s3_bucket_url

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
#define BLOCKPATHLEN 400
#define MAX_FILENAME_LEN 255
#define MONITOR_INTERVAL 10

#define NUMSUBDIR 1000

#define RECLAIM_TRIGGER 10000

#define NO_LL 0
#define IS_DIRTY 1
#define TO_BE_DELETED 2
#define TO_BE_RECLAIMED 3
#define RECLAIMED 4

#define DEFAULT_CONFIG_PATH "/etc/hcfs.conf"

#endif  /* GW20_SRC_PARAMS_H_ */

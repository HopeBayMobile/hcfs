#include "sys/stat.h"
#include "fuseop.h"
#include "params.h"
#include "meta.h"

#ifndef META_PROCESSING_MOCK_PARAMS_H_
#define META_PROCESSING_MOCK_PARAMS_H_
/* Parameters for mock functions */

#define ROOT_INODE 543783

/* System statistics */
#define MOCK_SYSTEM_SIZE 1048576
#define MOCK_SYSTEM_META_SIZE 104857
#define MOCK_CACHE_SIZE 1048576
#define MOCK_CACHE_BLOCKS 100

#define MOCK_BLOCK_SIZE 1024

/* User-defined parameters */
#define PARAM_MAX_BLOCK_SIZE 1024

/* decrease_nlink_inode_file() & seek_page() */
#define NUM_BLOCKS 32

/* dir_remove_entry() */
#define TOTAL_CHILDREN_NUM 23
#define LINK_NUM 12
DIR_META_TYPE to_verified_meta;
HCFS_STAT to_verified_stat;

/* delete_inode_meta() */
#define INO_RENAME_SUCCESS 3
#define INO_RENAME_FAIL 4566789
#define TO_DELETE_METAPATH "mock_to_delete_meta"
#define MOCK_META_PATH "mock_this_meta"

/* add_dir_entry() */
#define INO_INSERT_DIR_ENTRY_FAIL 20
#define INO_INSERT_DIR_ENTRY_SUCCESS_WITHOUT_SPLITTING 21
#define INO_INSERT_DIR_ENTRY_SUCCESS_WITH_SPLITTING 22

/* actual_delete_inode() */
#define INO_DELETE_FILE_BLOCK 200000
#define INO_DELETE_DIR 12345
#define INO_DELETE_LNK 13142
#define TRUNC_SIZE 65536

char pathlookup_write_parent_success;
char delete_pathcache_node_success;

/* fetch_inode_stat() & fetch_xattr_page() */
#define INO_REGFILE 12213
#define INO_FIFO 34534
#define INO_DIR 9403
#define INO_LNK 8234
#define INO_DIR_XATTR_PAGE_EXIST 14423
#define INO_LNK_XATTR_PAGE_EXIST 23345
#define INO_REGFILE_XATTR_PAGE_EXIST 8904
#define GENERATION_NUM 5

/* mknod_update_meta() & mkdir_update_meta() */
#define INO_META_CACHE_UPDATE_FILE_SUCCESS 5
#define INO_META_CACHE_UPDATE_FILE_FAIL 6
#define INO_DIR_ADD_ENTRY_SUCCESS 7
#define INO_DIR_ADD_ENTRY_FAIL 8
#define INO_META_CACHE_UPDATE_DIR_SUCCESS 9
#define INO_META_CACHE_UPDATE_DIR_FAIL 10

/* unlink_update_meta() & rmdir_update_meta() */
#define INO_DIR_REMOVE_ENTRY_SUCCESS 3
#define INO_DIR_REMOVE_ENTRY_FAIL 4
#define INO_CHILDREN_IS_EMPTY 5
#define INO_CHILDREN_IS_NONEMPTY 6

/* link_update_meta() */
#define INO_TOO_MANY_LINKS 279348

/* inherit_xattr() */
#define INO_NO_XATTR_PAGE 758925
#define INO_XATTR_PAGE_EXIST 490013

#define INO_CHECK_LOC_DIR 12345
#define INO_CHECK_LOC_FILE 12543

size_t TOTAL_KEY_SIZE;
size_t XATTR_VALUE_SIZE; 
char xattr_key[3][100];
char global_mock_namespace;
int32_t xattr_count;

/* pin_inode() */
char collect_dir_children_flag;

/* change_pin_falg */
char pin_flag_in_meta;
char test_change_pin_flag;

/*
	INO_SEEK_DIR_ENTRY_OK - meta_cache_seek_dir_entry() return 0 and result_index>=0
	INO_SEEK_DIR_ENTRY_NOTFOUND - meta_cache_seek_dir_entry() return 0 and result_index<0
	IND_SEEK_DIR_ENTRY_FAIL - meta_cache_seek_dir_entry() return -1 
*/
enum { INO_SEEK_DIR_ENTRY_OK,
	INO_SEEK_DIR_ENTRY_NOTFOUND,
	INO_SEEK_DIR_ENTRY_FAIL
};


/*
	INO_LOOKUP_FILE_DATA_OK - meta_cache_lookup_file_data() return 0
	INO_LOOKUP_FILE_DATA_OK_NO_BLOCK_PAGE - meta_cache_lookup_file_data() return 0
	                                        and next_block_page = 0
	IND_LOOKUP_FILE_DATA_FAIL - meta_cache_lookup_file_data() return -1 
	INO_UPDATE_FILE_DATA_FAIL - meta_cache_lookup_file_data() return 0 and 
	                            meta_cache_update_file_data() return -1
*/
/* seek_page() & create_page() */
enum { INO_LOOKUP_FILE_DATA_OK,
	INO_UPDATE_FILE_DATA_FAIL,
	INO_DIRECT_SUCCESS,
	INO_SINGLE_INDIRECT_SUCCESS,
	INO_DOUBLE_INDIRECT_SUCCESS,
	INO_TRIPLE_INDIRECT_SUCCESS,
	INO_QUADRUPLE_INDIRECT_SUCCESS,
	INO_CREATE_PAGE_SUCCESS
};

/*
	INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_1 - meta_cache_lookup_dir_data() return 0 and st_nlink = 1
	INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_2 - meta_cache_lookup_dir_data() return 0 and st_nlink = 2
	INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel - meta_cache_lookup_dir_data() return 0 and st_nlink = 1
	INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel - meta_cache_lookup_dir_data() return 0 and st_nlink = 1
*/
enum { INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_1,
	INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_2,
	INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel,
	INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel,
	INO_LOOKUP_FILE_DATA_OK_LOCK_ENTRY_FAIL
};

/*
	Mock data for super_block_unittest
 */
SYSTEM_CONF_STRUCT *system_config;

/*
	Tool vars used for lookup_count_unittest	
 */
char *check_actual_delete_table;

/* meta_cache_set_uploading_info */
char CHECK_UPLOADING_FLAG;
long long CHECK_TOUPLOAD_BLOCKS;

#define MOCK_UPLOAD_SEQ 1234
long long CHECK_UPLOAD_SEQ;

/* Used in pinning_worker() */
char FINISH_PINNING;

enum {
	INO_PINNING_ENOSPC,
	INO_PINNING_EIO,
	INO_PINNING_ENOENT,
	INO_PINNING_ESHUTDOWN
};

/* Used in pinning_loop() */
#define TOTAL_MOCK_INODES 50
ino_t mock_inodes[TOTAL_MOCK_INODES];
int32_t mock_inodes_counter;

ino_t verified_inodes[TOTAL_MOCK_INODES];
int32_t verified_inodes_counter;
sem_t verified_inodes_sem;

#endif

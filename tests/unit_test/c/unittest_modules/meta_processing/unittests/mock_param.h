/* Parameters for mock functions */
#define MOCK_SYSTEM_SIZE 1048576
#define MOCK_CACHE_SIZE 1048576
#define MOCK_CACHE_BLOCKS 100

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
enum { INO_LOOKUP_FILE_DATA_OK,
	INO_LOOKUP_FILE_DATA_OK_NO_BLOCK_PAGE,
	INO_LOOKUP_FILE_DATA_FAIL,
	INO_UPDATE_FILE_DATA_FAIL
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
	INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel
};

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
	INO_LOOKUP_FILE_DATA_OK_NO_BLOCK_PAGE - meta_cache_lookup_file_data() return 0 and next_block_page = 0
	IND_LOOKUP_FILE_DATA_FAIL - meta_cache_lookup_file_data() return -1 
*/
enum { INO_LOOKUP_FILE_DATA_OK,
	INO_LOOKUP_FILE_DATA_OK_NO_BLOCK_PAGE,
	INO_LOOKUP_FILE_DATA_FAIL
};

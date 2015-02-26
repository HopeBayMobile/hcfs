/*
	INO_SEEK_DIR_ENTRY_OK - meta_cache_seek_dir_entry() return 0 and result_index>=0
	INO_SEEK_DIR_ENTRY_NOTFOUND - meta_cache_seek_dir_entry() return 0 and result_index<0
	IND_SEEK_DIR_ENTRY_FAIL - meta_cache_seek_dir_entry() return -1 
*/
enum { INO_SEEK_DIR_ENTRY_OK,
	INO_SEEK_DIR_ENTRY_NOTFOUND,
	INO_SEEK_DIR_ENTRY_FAIL
};

#include <sys/stat.h>

#define TMP_META_DIR "/tmp/tmp_meta_dir"
#define TMP_META_FILE_PATH "/tmp/tmp_meta_dir/tmp_meta"
/*
	INO__FETCH_META_PATH_FAIL causes mock fetch_meta_path() fail and return -1
	INO__FETCH_META_PATH_SUCCESS makes mock fetch_meta_path() success and return 0
	INO__FETCH_META_PATH_ERR makes mock fetch_meta_path() return 0 but fill char *path with nothing
 */
enum { INO__FETCH_META_PATH_FAIL, 
	INO__FETCH_META_PATH_SUCCESS, 
	INO__FETCH_META_PATH_ERR };

HCFS_STAT *generate_mock_stat(ino_t inode_num);

char MOCK_FINISH_UPLOADING;
char MOCK_RETURN_VAL;

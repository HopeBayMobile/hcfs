#include <inttypes.h>

#include "fuseop.h"
#include "FS_manager.h"
#include "meta.h"
#include "metaops.h"

/* Private declaration */
#define LIST_DIR_LIMIT 1000

#define ERROR_SYSCALL       -1
#define ERROR_UNSUPPORT_VER -2


typedef struct {
	char is_walk_end;
	int64_t end_page_pos;
	int32_t end_el_no;
	int32_t num_walked;
} TREE_WALKER;

/* Public declaration */

typedef struct {
	uint64_t inode;
	char d_name[256];
	uint8_t d_type;
} _PACKED PORTABLE_DIR_ENTRY;

typedef struct {
	uint64_t block_num;
	uint64_t block_seq;
} _PACKED PORTABLE_BLOCK_NAME;

typedef struct {
	int32_t result;
	int32_t file_type;
	uint64_t child_number;
	HCFS_STAT_v1 stat;
} _PACKED RET_META;

int32_t list_volume(const char *meta_path,
			     PORTABLE_DIR_ENTRY **ptr_ret_entry,
			     uint64_t *ret_num);

void parse_meta(const char *meta_path, RET_META *meta);

int32_t list_dir_inorder(const char *meta_path, const int64_t page_pos,
			 const int32_t start_el, const int32_t limit,
			 int64_t *end_page_pos, int32_t *end_el_no,
			 int32_t *num_children, PORTABLE_DIR_ENTRY *file_list);

int32_t get_vol_usage(const char *meta_path, int64_t *vol_usage);
int32_t list_file_blocks_v1(const char *meta_path,
			 PORTABLE_BLOCK_NAME **block_list_ptr,
			 int64_t *ret_num, int64_t *inode_num);

/* START of external reference */
int64_t seek_page2(FILE_META_TYPE *temp_meta,
		   FILE *fptr,
		   int64_t target_page,
		   int64_t hint_page);
int64_t _load_indirect(int64_t target_page, FILE_META_TYPE *temp_meta,
			FILE *fptr, int32_t level);
int32_t check_page_level(int64_t page_index);
int64_t longpow(int64_t base, int32_t power);
/* END of external reference */

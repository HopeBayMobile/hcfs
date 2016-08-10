#include <inttypes.h>

#include "fuseop.h"
#include "FS_manager.h"
#include "meta.h"

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
} PORTABLE_DIR_ENTRY;

typedef struct {
	int32_t result;
	int32_t file_type;
	uint64_t child_number;
	HCFS_STAT_v1 stat;
} RET_META;

int32_t list_external_volume(const char *meta_path,
			     PORTABLE_DIR_ENTRY **ptr_ret_entry,
			     uint64_t *ret_num);

void parse_meta(const char *meta_path, RET_META *meta);

int32_t list_dir_inorder(const char *meta_path, const int64_t page_pos,
			 const int32_t start_el, const int32_t limit,
			 int64_t *end_page_pos, int32_t *end_el_no,
			 PORTABLE_DIR_ENTRY *file_list);

/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: api_interface.c
* Abstract:
*	The c source file for python API, function are expected to run
*	without hcfs daemon as a independent .so library.
*
* Revision History
*	2016/6/16 Jethro add list_external_volume from list_filesystem
*
**************************************************************************/

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include "parser.h"

/**
 * @brief Test all functions with test data, if compiled
 *
 * ddd
 * @param param1 Description of the first parameter of the function.
 * @param param2 The second one, which follows @p param1.
 * @return Describe what the function returns.
 */
int main(void)
{
	PORTABLE_DIR_ENTRY *ret_entry;
	uint64_t ret_num;

	list_external_volume("testdata/fsmgr", &ret_entry, &ret_num);
	for (int i = 0; i < ret_num; i++)
		printf("%lu %s\n", ret_entry[i].inode, ret_entry[i].name);
}

/************************************************************************
*
* Function name: list_external_volume
*        Inputs: int32_t buf_num,
*                DIR_ENTRY *ret_entry,
*                int32_t *ret_num
*       Summary: load fsmgr file and return all external volume
*
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t list_external_volume(char *fs_mgr_path,
			     PORTABLE_DIR_ENTRY **entry_array_ptr,
			     uint64_t *ret_num)
{
	DIR_ENTRY_PAGE tpage;
	DIR_META_TYPE tmp_head;
	int64_t num_walked;
	int64_t next_node_pos;
	int32_t count;
	int32_t FS_list_fh = open(fs_mgr_path, O_RDWR);
	ssize_t return_code;
	PORTABLE_DIR_ENTRY *ret_entry;

	return_code = pread(FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE), 16);
	if (return_code == -1)
		return return_code;

	/* Initialize B-tree walk by first loading the first node of the
	 * tree walk.
	 */
	next_node_pos = tmp_head.tree_walk_list_head;
	num_walked = 0;
	while (next_node_pos != 0) {
		return_code = pread(FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (return_code == -1)
			return return_code;
		num_walked += tpage.num_entries;
		next_node_pos = tpage.tree_walk_next;
	}
	/* allocate memory for dir_entries */
	ret_entry = (PORTABLE_DIR_ENTRY *)malloc(sizeof(PORTABLE_DIR_ENTRY) *
						 num_walked);
	*entry_array_ptr = ret_entry;

	/* load dir_entries */
	next_node_pos = tmp_head.tree_walk_list_head;
	num_walked = 0;
	while (next_node_pos != 0) {
		return_code = pread(FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
				    next_node_pos);
		if (return_code == -1)
			return return_code;
		for (count = 0; count < tpage.num_entries; count++) {
			switch (tpage.dir_entries[count].d_type) {
			case ANDROID_INTERNAL:
				break;
			case ANDROID_EXTERNAL:
			case ANDROID_MULTIEXTERNAL:
				ret_entry[num_walked].inode =
				    tpage.dir_entries[count].d_ino;
				strncpy(ret_entry[num_walked].name,
					tpage.dir_entries[count].d_name,
					sizeof(ret_entry[num_walked].name));
				num_walked++;
				break;
			default:
				assert(1);
			}
		}
		next_node_pos = tpage.tree_walk_next;
	}

	*ret_num = num_walked;

	return 0;
}

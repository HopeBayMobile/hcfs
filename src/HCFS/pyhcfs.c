/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: api_interface.c
* Abstract: The c source file for Defining API for controlling / monitoring
*
* Revision History
* 	2016/6/16 Jethro add list_external_volume from list_filesystem 
*
**************************************************************************/

#include <inttypes.h>

#include "fuseop.h"

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
int32_t list_external_volume(uint64_t buf_num, DIR_ENTRY *ret_entry,
		    uint64_t *ret_num)
{
	DIR_ENTRY_PAGE tpage;
	DIR_META_TYPE tmp_head;
	int32_t errcode;
	ssize_t ret_ssize;
	int64_t num_walked;
	int64_t next_node_pos;
	int32_t count;
	*entryarray = malloc(sizeof(DIR_ENTRY) * num_entries);
	ret = list_filesystem(num_entries, *entryarray, &temp);


	if (fs_mgr_head->num_FS > buf_num) {
		*ret_num = fs_mgr_head->num_FS;
		return 0;
	}

	/* If no filesystem */
	if (fs_mgr_head->num_FS <= 0) {
		*ret_num = 0;
		return 0;
	}

	PREAD(fs_mgr_head->FS_list_fh, &tmp_head, sizeof(DIR_META_TYPE), 16);

	/* Initialize B-tree walk by first loading the first node
		of the tree walk. */
	next_node_pos = tmp_head.tree_walk_list_head;

	num_walked = 0;

	while (next_node_pos != 0) {
		PREAD(fs_mgr_head->FS_list_fh, &tpage, sizeof(DIR_ENTRY_PAGE),
		      next_node_pos);
		if ((num_walked + tpage.num_entries) > (int64_t)buf_num) {
			/* Only compute the number of FS */
			num_walked += tpage.num_entries;
			next_node_pos = tpage.tree_walk_next;
			continue;
		}
		for (count = 0; count < tpage.num_entries; count++) {
			memcpy(&(ret_entry[num_walked]),
			       &(tpage.dir_entries[count]), sizeof(DIR_ENTRY));
			num_walked++;
		}
		next_node_pos = tpage.tree_walk_next;
	}

	if (((int64_t)fs_mgr_head->num_FS != num_walked) ||
		(tmp_head.total_children != num_walked)) {
		/* Number of FS is wrong. */
		write_log(0, "Error in FS num. Recomputing\n");
		fs_mgr_head->num_FS = num_walked;
		if (tmp_head.total_children != num_walked) {
			tmp_head.total_children = num_walked;
			write_log(0, "Rewriting FS num in database\n");
			PWRITE(fs_mgr_head->FS_list_fh, &tmp_head,
			       sizeof(DIR_META_TYPE), 16);
		}
	}

	*ret_num = num_walked;

	return 0;
errcode_handle:
	return errcode;
}

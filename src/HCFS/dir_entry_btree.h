/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dir_entry_btree.h
* Abstract: The c header file for B-tree operations in managing
*           directory entries.
*
* Revision History
* 2015/2/10 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/
#ifndef GW20_HCFS_DIR_ENTRY_BTREE_H_
#define GW20_HCFS_DIR_ENTRY_BTREE_H_

#include "fuseop.h"

int dentry_binary_search(const DIR_ENTRY *entry_array, const int num_entries,
			const DIR_ENTRY *new_entry, int *index_to_insert);

int search_dir_entry_btree(char *target_name, DIR_ENTRY_PAGE *tnode,
		int fh, int *result_index, DIR_ENTRY_PAGE *result_node);

/* if returns 1, then there is an entry to be added to the parent */
int insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode,
	int fh, DIR_ENTRY *overflow_median, long long *overflow_new_page,
	DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	long long *temp_child_page_pos);

int delete_dir_entry_btree(DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *tnode,
	int fh, DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	long long *temp_child_page_pos);

int rebalance_btree(DIR_ENTRY_PAGE *tnode, int fh, DIR_META_TYPE *this_meta,
	int selected_child, DIR_ENTRY *tmp_entries,
	long long *temp_child_page_pos);

int extract_largest_child(DIR_ENTRY_PAGE *tnode, int fh,
	DIR_META_TYPE *this_meta, DIR_ENTRY *extracted_child,
	DIR_ENTRY *tmp_entries, long long *temp_child_page_pos);

#endif  /* GW20_HCFS_DIR_ENTRY_BTREE_H_ */

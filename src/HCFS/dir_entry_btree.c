/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "dir_entry_btree.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>

#include "macro.h"
#include "global.h"
#include "params.h"
#include "logger.h"
#include "utils.h"
#include "fuseop.h"

/* TODO: How to integrate dir page reading / updating with mem cache? */
/* TODO: Revisit how to reduce IO for node updating and GC */
/* TODO: Remove doubly linked list struct for tree_walk (no use) */

/************************************************************************
*
* Function name: dentry_binary_search
*        Inputs: DIR_ENTRY *entry_array, int32_t num_entries,
*                DIR_ENTRY *new_entry, int32_t *index_to_insert
*       Summary: Binary search the directory entry element (new_entry) in
*                "entry_array". "num_entries" indicates the number of
*                entries in "entry_array".
*  Return value: Index in entry_array if found, or -1 if not. If not found,
*                (*index_to_insert) returns where in entry_array the new
*                element should be inserted.
*
*************************************************************************/
int32_t dentry_binary_search(const DIR_ENTRY *entry_array, const int32_t num_entries,
			const DIR_ENTRY *new_entry, int32_t *index_to_insert,
			BOOL is_external)
{
	int32_t compare_entry, compare_result;
	int32_t start_index, end_index;

	if (num_entries < 1) {
		*index_to_insert = 0;
		/*Insert or traverse before the first element*/
		return -1;
	}

	start_index = 0;
	end_index = num_entries;

	while (end_index > start_index) {
		/*If there is something left to compare to*/

		 /*Index of the element to compare to */
		if (end_index == (start_index + 1))
			compare_entry = start_index;
		else
			compare_entry = (end_index + start_index) / 2;

		if (compare_entry < 0)
			compare_entry = 0;
		if (compare_entry >= num_entries)
			compare_entry = num_entries - 1;

		if (is_external == TRUE)
			compare_result = strcasecmp(new_entry->d_name,
					entry_array[compare_entry].d_name);
		else
			compare_result = strcmp(new_entry->d_name,
					entry_array[compare_entry].d_name);

		/* If entry is the same */
		if (compare_result == 0)
			return compare_entry;

		/* If compare_result < 0, new element belongs to the left
			of the entry being compared to */
		if (compare_result < 0)
			end_index = compare_entry;
		else
			start_index = compare_entry + 1;
	}

	*index_to_insert = start_index;
	return -1;  /*Not found. Returns where to insert the new entry*/
}

/************************************************************************
*
* Function name: search_dir_entry_btree
*        Inputs: const char *target_name, DIR_ENTRY_PAGE *tnode,
		int32_t fh, int32_t *result_index, DIR_ENTRY_PAGE *result_node
*       Summary: Recursive search routine for B-tree search.
*                Searches for the directory entry with name "target_name"
*                in B-tree node "tnode". If found, returns the result by
*                returning the tree node with the target index in the node
*                ("result_node" and "result_index"). "fh" is the file
*                handle for the meta file.
*  Return value: Target index in the node if found. If not found, returns
*                -ENOENT. If error, return negation of error code;
*
*************************************************************************/
int32_t search_dir_entry_btree(const char *target_name, DIR_ENTRY_PAGE *tnode,
		int32_t fh, int32_t *result_index, DIR_ENTRY_PAGE *result_node,
		BOOL is_external)
{
	DIR_ENTRY temp_entry;
	int32_t s_index, ret_val;
	DIR_ENTRY_PAGE temp_page;

	strcpy(temp_entry.d_name, target_name);

	ret_val = dentry_binary_search(tnode->dir_entries,
		tnode->num_entries, &temp_entry, &s_index, is_external);

	if (ret_val >= 0) {
		*result_index = ret_val;
		memcpy(result_node, tnode, sizeof(DIR_ENTRY_PAGE));
		return ret_val;
	}

	/* Not in the current node. First check if we can dig deeper*/

	/* If already at leaf, return -1 (item not found) */
	if (tnode->child_page_pos[s_index] == 0)
		return -ENOENT;

	/*Load child node*/
	PREAD(fh, &temp_page, sizeof(DIR_ENTRY_PAGE),
			tnode->child_page_pos[s_index]);

	return search_dir_entry_btree(target_name, &temp_page, fh,
					result_index, result_node,
					is_external);
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: insert_dir_entry_btree
*        Inputs: DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode, int32_t fh,
*                DIR_ENTRY *overflow_median, int64_t *overflow_new_page,
*                DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
*                int64_t *temp_child_page_pos
*       Summary: Recursive insertion routine for B-tree search.
*                Insert the directory entry "new_entry" to B-tree. "tnode"
*                is the B-tree node currently being processed. "fh" is the
*                file handle of the meta file. "this_meta" is the meta head
*                for the directory object.
*                If in B-tree insertion, an overflow of node entries occurs
*                and the node needs to be splitted into two, the position
*                of the new node in the meta file is returned by the pointer
*                "overflow_new_page", and the median of the old node (which
*                is to be inserted to the parent of the old node) is copied
*                to the entry pointed by "overflow_median".
*                To conserve space, caller to this function has to reserve
*                temp space for processing, pointed by "tmp_entries" and
*                "temp_child_page_pos".
*
*  Return value: 0 if the insertion is completed without node splitting.
*                1 if the insertion is completed with node splitting (overflow).
*                -EEXIST if the entry to be inserted already exists in the tree.
*                negation of error code if an error occurs.
*
*************************************************************************/
int32_t insert_dir_entry_btree(DIR_ENTRY *new_entry, DIR_ENTRY_PAGE *tnode,
	int32_t fh, DIR_ENTRY *overflow_median, int64_t *overflow_new_page,
	DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
	int64_t *temp_child_page_pos, BOOL is_external, int64_t meta_pos)
{
	int32_t s_index, ret_val, median_entry;
	DIR_ENTRY_PAGE newpage, temppage, temp_page2;
	DIR_ENTRY tmp_overflow_median;
	int32_t temp_total;
	int64_t tmp_overflow_new_page;
	size_t tmp_size;
	off_t ret_pos;

	/*First search for the index to insert or traverse*/
	/* s_index: selected index to insert */
	ret_val = dentry_binary_search(tnode->dir_entries,
		tnode->num_entries, new_entry, &s_index, is_external);

	/*If entry already in the tree, return error */
	if (ret_val >= 0)
		return -EEXIST;

	if (tnode->child_page_pos[s_index] == 0) {
		/*We are now at the leaf node*/
		if (tnode->num_entries < MAX_DIR_ENTRIES_PER_PAGE) {
			/*Can add new entry to this node*/
			/*First shift the elements to the right of the point
				to insert*/
			if (s_index < tnode->num_entries) {
				tmp_size = sizeof(DIR_ENTRY) *
						(tnode->num_entries - s_index);
				memcpy(&(tmp_entries[0]),
					&(tnode->dir_entries[s_index]),
								tmp_size);
				memcpy(&(tnode->dir_entries[s_index+1]),
					&(tmp_entries[0]), tmp_size);
			}
			/*Insert the new element*/
			memcpy(&(tnode->dir_entries[s_index]), new_entry,
							sizeof(DIR_ENTRY));

			(tnode->num_entries)++;
			write_log(10, "Insert node. %d, %ld\n",
				tnode->num_entries, tnode->this_page_pos);
			PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
							tnode->this_page_pos);
			return 0; /*Insertion completed*/
		}

		/* Need to split. */
		if (s_index > 0)
			memcpy(tmp_entries, tnode->dir_entries,
						sizeof(DIR_ENTRY) * s_index);
		memcpy(&(tmp_entries[s_index]), new_entry, sizeof(DIR_ENTRY));

		if (s_index < tnode->num_entries) {
			tmp_size = sizeof(DIR_ENTRY) *
						(tnode->num_entries - s_index);
			memcpy(&(tmp_entries[s_index+1]),
				&(tnode->dir_entries[s_index]), tmp_size);
		}

		/* Select median */
		median_entry = (tnode->num_entries + 1) / 2;
		temp_total = tnode->num_entries + 1;

		/* Copy the median */
		memcpy(overflow_median, &(tmp_entries[median_entry]),
							sizeof(DIR_ENTRY));

		/* Copy items to the left of median to the old node*/
		tnode->num_entries = median_entry;
		memcpy(tnode->dir_entries, tmp_entries,
					sizeof(DIR_ENTRY) * median_entry);

		/* Create a new node and copy all items to the right of
			median to the new node */
		if (this_meta->entry_page_gc_list != 0) {
			/*Reclaim node from gc list first*/
			PREAD(fh, &newpage, sizeof(DIR_ENTRY_PAGE),
						this_meta->entry_page_gc_list);
			newpage.this_page_pos = this_meta->entry_page_gc_list;
			this_meta->entry_page_gc_list = newpage.gc_list_next;
		} else {
			/* Check if meta space is enough or not. */
			if (NO_META_SPACE())
				return -ENOSPC;

			memset(&newpage, 0, sizeof(DIR_ENTRY_PAGE));
			ret_pos = LSEEK(fh, 0, SEEK_END);
			newpage.this_page_pos = ret_pos;
		}
		newpage.gc_list_next = 0;
		newpage.tree_walk_next = this_meta->tree_walk_list_head;
		newpage.tree_walk_prev = 0;

		if (this_meta->tree_walk_list_head == tnode->this_page_pos) {
			tnode->tree_walk_prev = newpage.this_page_pos;
		} else {
			PREAD(fh, &temp_page2, sizeof(DIR_ENTRY_PAGE),
						this_meta->tree_walk_list_head);
			temp_page2.tree_walk_prev = newpage.this_page_pos;
			PWRITE(fh, &temp_page2, sizeof(DIR_ENTRY_PAGE),
						this_meta->tree_walk_list_head);
		}

		/* Write current node to disk */
		PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
							tnode->this_page_pos);

		this_meta->tree_walk_list_head = newpage.this_page_pos;
		PWRITE(fh, this_meta, sizeof(DIR_META_TYPE), meta_pos);

		/* Parent of new node is the same as the parent of the old
			node */
		newpage.parent_page_pos = tnode->parent_page_pos;
		memset(newpage.child_page_pos, 0, sizeof(int64_t) *
						(MAX_DIR_ENTRIES_PER_PAGE+1));
		newpage.num_entries = temp_total - median_entry - 1;
		memcpy(newpage.dir_entries, &(tmp_entries[median_entry+1]),
				sizeof(DIR_ENTRY) * newpage.num_entries);

		/* Write to disk after finishing */
		PWRITE(fh, &newpage, sizeof(DIR_ENTRY_PAGE),
						newpage.this_page_pos);

		/* Pass the median and the file pos of the new node to
			the parent*/
		*overflow_new_page = newpage.this_page_pos;
		write_log(10, "overflow %s\n", overflow_median->d_name);

		return 1;
	}
	/* Internal node. Prepare to go deeper */
	PREAD(fh, &temppage, sizeof(DIR_ENTRY_PAGE),
					tnode->child_page_pos[s_index]);
	ret_val = insert_dir_entry_btree(new_entry, &temppage, fh,
				&tmp_overflow_median, &tmp_overflow_new_page,
				this_meta, tmp_entries, temp_child_page_pos,
				is_external, meta_pos);

	/*If finished. Just return*/
	if (ret_val < 1)
		return ret_val;

	write_log(10, "overflow up %s\n", tmp_overflow_median.d_name);

	/* Reload current node */
	PREAD(fh, tnode, sizeof(DIR_ENTRY_PAGE), tnode->this_page_pos);

	/* If function return contains a median, insert to the current node */
	if (tnode->num_entries < MAX_DIR_ENTRIES_PER_PAGE) {
		write_log(10, "overflow up path a %s\n",
				tmp_overflow_median.d_name);
		/*Can add new entry to this node*/
		/*First shift the elements to the right of the point
			to insert*/
		if (s_index < tnode->num_entries) {
			tmp_size = sizeof(DIR_ENTRY) *
						(tnode->num_entries - s_index);
			memcpy(&(tmp_entries[0]),
				&(tnode->dir_entries[s_index]),	tmp_size);
			memcpy(&(tnode->dir_entries[s_index+1]),
						&(tmp_entries[0]), tmp_size);

			tmp_size = sizeof(int64_t) *
						(tnode->num_entries - s_index);
			memcpy(&(temp_child_page_pos[0]),
				&(tnode->child_page_pos[s_index+1]), tmp_size);
			memcpy(&(tnode->child_page_pos[s_index+2]),
				&(temp_child_page_pos[0]), tmp_size);
		}
		/*Insert the overflow element*/
		memcpy(&(tnode->dir_entries[s_index]),
				&tmp_overflow_median, sizeof(DIR_ENTRY));
		tnode->child_page_pos[s_index+1] = tmp_overflow_new_page;

		(tnode->num_entries)++;
		PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
							tnode->this_page_pos);
		return 0; /*Insertion completed*/
	}

	write_log(0, "overflow up path b %s\n", tmp_overflow_median.d_name);

	/*Need to split*/
	if (s_index > 0)
		memcpy(tmp_entries, tnode->dir_entries,
						sizeof(DIR_ENTRY) * s_index);
	memcpy(&(tmp_entries[s_index]), &tmp_overflow_median,
							sizeof(DIR_ENTRY));
	if (s_index < tnode->num_entries) {
		tmp_size = sizeof(DIR_ENTRY) * (tnode->num_entries - s_index);
		memcpy(&(tmp_entries[s_index+1]),
				&(tnode->dir_entries[s_index]), tmp_size);
	}

	if (s_index > 0)
		memcpy(temp_child_page_pos, tnode->child_page_pos,
						sizeof(int64_t)*(s_index+1));

	temp_child_page_pos[s_index+1] = tmp_overflow_new_page;
	if (s_index < tnode->num_entries) {
		tmp_size = sizeof(int64_t) * (tnode->num_entries - s_index);
		memcpy(&(temp_child_page_pos[s_index+2]),
				&(tnode->child_page_pos[s_index+1]), tmp_size);
	}

	/* Select median */
	median_entry = (tnode->num_entries + 1) / 2;
	temp_total = tnode->num_entries + 1;

	/* Copy the median */
	memcpy(overflow_median, &(tmp_entries[median_entry]),
							sizeof(DIR_ENTRY));

	/* Copy items to the left of median to the old node */
	tnode->num_entries = median_entry;
	memcpy(tnode->dir_entries, tmp_entries,
					sizeof(DIR_ENTRY) * median_entry);
	memcpy(tnode->child_page_pos, temp_child_page_pos,
					sizeof(int64_t) * (median_entry+1));

	/* Create a new node and copy all items to the right of median
		to the new node */
	if (this_meta->entry_page_gc_list != 0) {
		/*Reclaim node from gc list first*/
		PREAD(fh, &newpage, sizeof(DIR_ENTRY_PAGE),
					this_meta->entry_page_gc_list);
		newpage.this_page_pos = this_meta->entry_page_gc_list;
		this_meta->entry_page_gc_list = newpage.gc_list_next;
	} else {
		memset(&newpage, 0, sizeof(DIR_ENTRY_PAGE));
		ret_pos = LSEEK(fh, 0, SEEK_END);
		newpage.this_page_pos = ret_pos;
	}
	newpage.gc_list_next = 0;
	newpage.tree_walk_next = this_meta->tree_walk_list_head;
	newpage.tree_walk_prev = 0;

	if (this_meta->tree_walk_list_head == tnode->this_page_pos) {
		tnode->tree_walk_prev = newpage.this_page_pos;
	} else {
		PREAD(fh, &temp_page2, sizeof(DIR_ENTRY_PAGE),
					this_meta->tree_walk_list_head);
		temp_page2.tree_walk_prev = newpage.this_page_pos;
		PWRITE(fh, &temp_page2, sizeof(DIR_ENTRY_PAGE),
					this_meta->tree_walk_list_head);
	}

	/*Write current node to disk*/
	PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE), tnode->this_page_pos);

	this_meta->tree_walk_list_head = newpage.this_page_pos;
	PWRITE(fh, this_meta, sizeof(DIR_META_TYPE), meta_pos);

	/* Parent of new node is the same as the parent of the old node*/
	newpage.parent_page_pos = tnode->parent_page_pos;
	memset(newpage.child_page_pos, 0,
			sizeof(int64_t) * (MAX_DIR_ENTRIES_PER_PAGE+1));
	newpage.num_entries = temp_total - median_entry - 1;
	memcpy(newpage.dir_entries, &(tmp_entries[median_entry+1]),
				sizeof(DIR_ENTRY) * newpage.num_entries);
	memcpy(newpage.child_page_pos, &(temp_child_page_pos[median_entry+1]),
				sizeof(int64_t)*(newpage.num_entries+1));

	/* Write to disk after finishing */
	PWRITE(fh, &newpage, sizeof(DIR_ENTRY_PAGE), newpage.this_page_pos);

	/* Pass the median and the file pos of the new node to the parent*/
	*overflow_new_page = newpage.this_page_pos;
	write_log(10, "overflow %s\n", overflow_median->d_name);

	return 1;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: delete_dir_entry_btree
*        Inputs: DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *tnode,
*                int32_t fh, DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
*                int64_t *temp_child_page_pos
*       Summary: Delete dir entry "to_delete_entry" from the B-tree. "tnode"
*                is the B-tree node currently being processed. "fh" is the
*                file handle of the meta file. "this_meta" is the meta head
*                for the directory object.
*
*                To conserve space, caller to this function has to reserve
*                temp space for processing, pointed by "tmp_entries" and
*                "temp_child_page_pos".
*
*  Return value: 0 if deleted successfully. Otherwise return negation of error
*                code.
*
*************************************************************************/
int32_t delete_dir_entry_btree(DIR_ENTRY *to_delete_entry, DIR_ENTRY_PAGE *tnode,
		int32_t fh, DIR_META_TYPE *this_meta, DIR_ENTRY *tmp_entries,
		int64_t *temp_child_page_pos, BOOL is_external,
		int64_t meta_pos)
{
	int32_t s_index, ret_val, entry_to_delete;
	DIR_ENTRY_PAGE temppage;
	DIR_ENTRY extracted_child;
	size_t tmp_size;

	/* First search for the index to insert or traverse */
	entry_to_delete = dentry_binary_search(tnode->dir_entries,
		tnode->num_entries, to_delete_entry, &s_index, is_external);

	if (entry_to_delete >= 0) {
		/* We found the element. Delete it. */

		if (tnode->child_page_pos[entry_to_delete] == 0) {
			/*We are now at the leaf node*/
			/* Just delete and return. Won't need to handle
				underflow here */
			tmp_size = sizeof(DIR_ENTRY) *
				((tnode->num_entries - entry_to_delete) - 1);
			memcpy(&(tmp_entries[0]),
				&(tnode->dir_entries[entry_to_delete+1]),
								tmp_size);
			memcpy(&(tnode->dir_entries[entry_to_delete]),
						&(tmp_entries[0]), tmp_size);
			tnode->num_entries--;

			PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
							tnode->this_page_pos);
			return 0;
		}
		/*Select and remove the largest element from the left
			subtree of entry_to_delete*/
		/* Conduct rebalancing all the way down */

		/* First make sure the selected child is balanced */

		ret_val = rebalance_btree(tnode, fh, this_meta,
					entry_to_delete, tmp_entries,
					temp_child_page_pos, meta_pos);

		/* If rebalanced, recheck by calling this function with
			the same parameters, else read the child node
						and go down the tree */
		/* If ret_val > 0, the current node is modified due to
		*  tree rebalancing, so will need to repeat the call */
		if (ret_val == 2) {
			/* Need to reload the current node.
				Old one is deleted */
			PREAD(fh, &temppage, sizeof(DIR_ENTRY_PAGE),
				this_meta->root_entry_page);
			return delete_dir_entry_btree(to_delete_entry,
				&temppage, fh, this_meta, tmp_entries,
						temp_child_page_pos,
						is_external, meta_pos);
		}

		if (ret_val > 0)
			return delete_dir_entry_btree(to_delete_entry, tnode,
					fh, this_meta, tmp_entries,
					temp_child_page_pos,
					is_external, meta_pos);

		if (ret_val < 0) {
			write_log(5,
				"debug dtree error in/after rebalancing\n");
			return ret_val;
		}

		PREAD(fh, &temppage, sizeof(DIR_ENTRY_PAGE),
					tnode->child_page_pos[entry_to_delete]);

		ret_val = extract_largest_child(&temppage, fh, this_meta,
			&extracted_child, tmp_entries, temp_child_page_pos,
			meta_pos);

		/* Replace the entry_to_delete with the largest element
			from the left subtree */
		if (ret_val < 0) {
			write_log(5,
				"debug error in finding largest child\n");
			return ret_val;
		}

		memcpy(&(tnode->dir_entries[entry_to_delete]),
					&extracted_child, sizeof(DIR_ENTRY));

		PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
							tnode->this_page_pos);
		return 0;
	}

	if (tnode->child_page_pos[s_index] == 0) {
		write_log(10, "debug dtree cannot find the item\n");
		return -ENOENT; /*Cannot find the item to delete. Return. */
	}

	/* Rebalance the selected child with its right sibling
		(or left if the rightmost) if needed */
	ret_val = rebalance_btree(tnode, fh, this_meta, s_index,
				tmp_entries, temp_child_page_pos, meta_pos);

	/* If rebalanced, recheck by calling this function with the same
		parameters, else read the child node and go down the tree */
	if (ret_val == 2) { /* Need to reload tnode. Old one is deleted */
		PREAD(fh, &temppage, sizeof(DIR_ENTRY_PAGE),
			this_meta->root_entry_page);
		return delete_dir_entry_btree(to_delete_entry, &temppage, fh,
				this_meta, tmp_entries, temp_child_page_pos,
				is_external, meta_pos);
	}

	if (ret_val > 0)
		return delete_dir_entry_btree(to_delete_entry, tnode, fh,
				this_meta, tmp_entries, temp_child_page_pos,
				is_external, meta_pos);

	if (ret_val < 0) {
		write_log(5, "debug dtree error in/after rebalancing\n");
		return ret_val;
	}

	PREAD(fh, &temppage, sizeof(DIR_ENTRY_PAGE),
						tnode->child_page_pos[s_index]);

	return delete_dir_entry_btree(to_delete_entry, &temppage, fh,
				this_meta, tmp_entries, temp_child_page_pos,
				is_external, meta_pos);

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: rebalance_btree
*        Inputs: DIR_ENTRY_PAGE *tnode, int32_t fh, DIR_META_TYPE *this_meta,
*                int32_t selected_child, DIR_ENTRY *tmp_entries,
*                int64_t *temp_child_page_pos
*       Summary: Check if the child "selected_child" of the B-tree node
*                "tnode" contains fewer elements than the lower bound after
*                deletion, so needs rebalancing.
*                If needs rebalancing, will pool the elements of this child
*                with one of this siblings.
*                "fh" is the file handle of the meta file. "this_meta" is
*                the meta head for the directory object.
*
*                To conserve space, caller to this function has to reserve
*                temp space for processing, pointed by "tmp_entries" and
*                "temp_child_page_pos".
*                "meta_pos" points to file offset of meta
*
*  Return value: 1 if rebalancing is conducted, and no new root is created.
*                2 if rebalancing is conducted, and there is a new root. The
*                   current node of the caller (tnode) should be reloaded
*                   in this case.
*                0 if no rebalancing is needed.
*                Negation of error code if an error occurred.
*
*************************************************************************/
int32_t rebalance_btree(DIR_ENTRY_PAGE *tnode, int32_t fh,
                        DIR_META_TYPE *this_meta, int32_t selected_child,
                        DIR_ENTRY *tmp_entries, int64_t *temp_child_page_pos,
                        int64_t meta_pos)
{
/* How to rebalance: if num_entries of child <= MIN_DIR_ENTRIES_PER_PAGE,
check if its right (or left) sibling contains child <
MAX_DIR_ENTRIES_PER_PAGE / 2. If so, just merge the two children and the
parent item in between (parent node lost one element). If the current node is
the root and has only one element, make the merged node the new root and put
the old root to the gc list.

If merging occurs, the dropped page goes to the gc list. Tree walk pointers
are also updated in this case.

If the sibling contains child > MAX_DIR_ENTRIES_PER_PAGE / 2, pool the elements
from the two nodes, plus the parent item in between, and split the pooled
elements into two, using the median as the new parent item. */

	int32_t selected_sibling, left_node, /*right_node,*/ to_return;
	DIR_ENTRY_PAGE left_page, right_page, temp_page;
	int32_t temp_total, median_entry;
	char merging;
	size_t tmp_size;

	/* Index out of bound */
	if (selected_child > tnode->num_entries || selected_child < 0)
		return -1;
	/* Leaf node needs not rebalance */
	if (tnode->child_page_pos[selected_child] <= 0)
		return -1;

	if (selected_child == tnode->num_entries) {
		/* If selected child is the rightmost one, sibling is
			the one to the left */
		PREAD(fh, &right_page, sizeof(DIR_ENTRY_PAGE),
					tnode->child_page_pos[selected_child]);

		if (right_page.num_entries > MIN_DIR_ENTRIES_PER_PAGE)
			return 0;		/* No rebalancing needed */

		selected_sibling = selected_child - 1;
		left_node = selected_sibling;
		/* right_node = selected_child; */
		PREAD(fh, &left_page, sizeof(DIR_ENTRY_PAGE),
				tnode->child_page_pos[selected_sibling]);
		if (left_page.num_entries < MAX_DIR_ENTRIES_PER_PAGE / 2)
			merging = TRUE;
		else
			merging = FALSE;
	} else {
		PREAD(fh, &left_page, sizeof(DIR_ENTRY_PAGE),
					tnode->child_page_pos[selected_child]);

		if (left_page.num_entries > MIN_DIR_ENTRIES_PER_PAGE)
			return 0;		/* No rebalancing needed */

		selected_sibling = selected_child + 1;
		left_node = selected_child;
		/* right_node = selected_sibling; */
		PREAD(fh, &right_page, sizeof(DIR_ENTRY_PAGE),
				tnode->child_page_pos[selected_sibling]);
		if (right_page.num_entries < MAX_DIR_ENTRIES_PER_PAGE / 2)
			merging = TRUE;
		else
			merging = FALSE;
	}

	/*First pool the items together */
	temp_total = left_page.num_entries + right_page.num_entries + 1;
	memcpy(&(tmp_entries[0]), &(left_page.dir_entries[0]),
				sizeof(DIR_ENTRY) * left_page.num_entries);
	memcpy(&(temp_child_page_pos[0]), &(left_page.child_page_pos[0]),
				sizeof(int64_t) * (left_page.num_entries+1));
	memcpy(&(tmp_entries[left_page.num_entries]),
			&(tnode->dir_entries[left_node]), sizeof(DIR_ENTRY));
	memcpy(&(tmp_entries[left_page.num_entries+1]),
				&(right_page.dir_entries[0]),
				sizeof(DIR_ENTRY) * right_page.num_entries);
	memcpy(&(temp_child_page_pos[left_page.num_entries+1]),
				&(right_page.child_page_pos[0]),
				sizeof(int64_t) * (right_page.num_entries+1));

	if (merging == TRUE) {
		/* Merge the two nodes and process node deletion */

		/* Copy the pooled items to the left node */
		memcpy(&(left_page.dir_entries[0]), &(tmp_entries[0]),
					sizeof(DIR_ENTRY) * temp_total);
		memcpy(&(left_page.child_page_pos[0]),
					&(temp_child_page_pos[0]),
					sizeof(int64_t) * (temp_total + 1));
		left_page.num_entries = temp_total;

		/* Drop the right node and update related info, including
			gc_list and tree walk pointer*/
		memset(&temp_page, 0, sizeof(DIR_ENTRY_PAGE));
		temp_page.this_page_pos = right_page.this_page_pos;
		temp_page.gc_list_next = this_meta->entry_page_gc_list;
		this_meta->entry_page_gc_list = temp_page.this_page_pos;
		PWRITE(fh, &temp_page, sizeof(DIR_ENTRY_PAGE),
						temp_page.this_page_pos);

		if (this_meta->tree_walk_list_head == right_page.this_page_pos)
			this_meta->tree_walk_list_head =
						right_page.tree_walk_next;

		if (right_page.tree_walk_next != 0) {
			if (right_page.tree_walk_next ==
						left_page.this_page_pos) {
				left_page.tree_walk_prev =
						right_page.tree_walk_prev;
			} else {
				if (right_page.tree_walk_next ==
							tnode->this_page_pos) {
					tnode->tree_walk_prev =
						right_page.tree_walk_prev;
				} else {
					PREAD(fh, &temp_page,
						sizeof(DIR_ENTRY_PAGE),
						right_page.tree_walk_next);
					temp_page.tree_walk_prev =
						right_page.tree_walk_prev;
					PWRITE(fh, &temp_page,
						sizeof(DIR_ENTRY_PAGE),
						right_page.tree_walk_next);
				}
			}
		}
		if (right_page.tree_walk_prev != 0) {
			if (right_page.tree_walk_prev ==
					left_page.this_page_pos) {
				left_page.tree_walk_next =
						right_page.tree_walk_next;
			} else {
				if (right_page.tree_walk_prev ==
							tnode->this_page_pos) {
					tnode->tree_walk_next =
						right_page.tree_walk_next;
				} else {
					PREAD(fh, &temp_page,
						sizeof(DIR_ENTRY_PAGE),
						right_page.tree_walk_prev);
					temp_page.tree_walk_next =
						right_page.tree_walk_next;
					PWRITE(fh, &temp_page,
						sizeof(DIR_ENTRY_PAGE),
						right_page.tree_walk_prev);
				}
			}
		}

		/*Decide whether we need to drop the root node and
			return 2, otherwise, update parent node*/
		if (tnode->num_entries == 1) {
			/* We are dropping the only element in the parent*/
			/* Drop root and make left_node the new root */
			memset(&temp_page, 0, sizeof(DIR_ENTRY_PAGE));
			temp_page.this_page_pos = tnode->this_page_pos;
			temp_page.gc_list_next = this_meta->entry_page_gc_list;
			this_meta->entry_page_gc_list = temp_page.this_page_pos;
			PWRITE(fh, &temp_page, sizeof(DIR_ENTRY_PAGE),
						temp_page.this_page_pos);

			if (this_meta->tree_walk_list_head ==
							tnode->this_page_pos)
				this_meta->tree_walk_list_head =
							tnode->tree_walk_next;

			if (tnode->tree_walk_next == left_page.this_page_pos) {
				left_page.tree_walk_prev =
							tnode->tree_walk_prev;
			} else {
				if (tnode->tree_walk_next != 0) {
					PREAD(fh, &temp_page,
						sizeof(DIR_ENTRY_PAGE),
						tnode->tree_walk_next);
					temp_page.tree_walk_prev =
						tnode->tree_walk_prev;
					PWRITE(fh, &temp_page,
						sizeof(DIR_ENTRY_PAGE),
						tnode->tree_walk_next);
				}
			}
			if (tnode->tree_walk_prev == left_page.this_page_pos) {
				left_page.tree_walk_next =
							tnode->tree_walk_next;
			} else {
				if (tnode->tree_walk_prev != 0) {
					PREAD(fh, &temp_page,
							sizeof(DIR_ENTRY_PAGE),
							tnode->tree_walk_prev);
					temp_page.tree_walk_next =
							tnode->tree_walk_next;
					PWRITE(fh, &temp_page,
							sizeof(DIR_ENTRY_PAGE),
							tnode->tree_walk_prev);
				}
			}
			this_meta->root_entry_page = left_page.this_page_pos;
			left_page.parent_page_pos = 0;
			to_return = 2;
		} else {
			to_return = 1;
			/* Just drop the item merged to the left node from
				tnode */
			tmp_size = sizeof(DIR_ENTRY) *
					(tnode->num_entries - (left_node + 1));
			memcpy(&(tmp_entries[0]),
				&(tnode->dir_entries[left_node+1]), tmp_size);
			memcpy(&(tnode->dir_entries[left_node]),
						&(tmp_entries[0]), tmp_size);

			tmp_size = sizeof(int64_t) *
					(tnode->num_entries - (left_node + 1));
			memcpy(&(temp_child_page_pos[0]),
			       &(tnode->child_page_pos[left_node+2]),
			       tmp_size);
			memcpy(&(tnode->child_page_pos[left_node+1]),
			       &(temp_child_page_pos[0]), tmp_size);
			tnode->num_entries--;

			PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
			       tnode->this_page_pos);
		}

		/* Write changes to left node and meta to disk and return */
		PWRITE(fh, this_meta, sizeof(DIR_META_TYPE), meta_pos);

		PWRITE(fh, &left_page, sizeof(DIR_ENTRY_PAGE),
		       left_page.this_page_pos);

		return to_return;
	}
	/* Split the pooled items into two, and replace the old parent
		in the middle with median */
	median_entry = temp_total / 2;

	/* Copy items to the left of the median to the left page
		and write to disk */
	memcpy(&(left_page.dir_entries[0]), &(tmp_entries[0]),
					sizeof(DIR_ENTRY) * median_entry);
	memcpy(&(left_page.child_page_pos[0]),
				&(temp_child_page_pos[0]),
				sizeof(int64_t) * (median_entry + 1));
	left_page.num_entries = median_entry;
	PWRITE(fh, &left_page, sizeof(DIR_ENTRY_PAGE),
						left_page.this_page_pos);

	/* Copy items to the right of the median to the right page
		and write to disk */
	memcpy(&(right_page.dir_entries[0]),
			&(tmp_entries[median_entry+1]),
			sizeof(DIR_ENTRY) * ((temp_total - median_entry)-1));
	memcpy(&(right_page.child_page_pos[0]),
			&(temp_child_page_pos[median_entry+1]),
			sizeof(int64_t) * (temp_total - median_entry));
	right_page.num_entries = (temp_total - median_entry)-1;
	PWRITE(fh, &right_page, sizeof(DIR_ENTRY_PAGE),
						right_page.this_page_pos);

	/* Write median to the current node and write to disk */
	memcpy(&(tnode->dir_entries[left_node]),
			&(tmp_entries[median_entry]), sizeof(DIR_ENTRY));
	PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
							tnode->this_page_pos);

	return 1;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: extract_largest_child
*        Inputs: DIR_ENTRY_PAGE *tnode, int32_t fh, DIR_META_TYPE *this_meta,
*                DIR_ENTRY *extracted_child, DIR_ENTRY *tmp_entries,
*                int64_t *temp_child_page_pos
*       Summary: Select and delete the largest element in this subtree.
*                "tnode" is the B-tree node being processed. "fh" is the
*                file handle of the meta file. "this_meta" is the meta head
*                for the directory object. If the largest element is found,
*                it is copied to "extracted_child" before being deleted from
*                the original node.
*                This function will also conduct tree rebalancing along the
*                path down to the leaf.
*
*                To conserve space, caller to this function has to reserve
*                temp space for processing, pointed by "tmp_entries" and
*                "temp_child_page_pos".
*
*  Return value: 0 if successful.
*                Negation of error code if an error occurred.
*
*************************************************************************/
int32_t extract_largest_child(DIR_ENTRY_PAGE *tnode, int32_t fh,
			DIR_META_TYPE *this_meta, DIR_ENTRY *extracted_child,
			DIR_ENTRY *tmp_entries, int64_t *temp_child_page_pos,
			int64_t meta_pos)
{
	/*Select and remove the largest element from the left subtree of
		entry_to_delete*/
	/* Conduct rebalancing all the way down, using rebalance_btree
		function */
	/* Return the largest element using extracted_child pointer */

	int32_t s_index, ret_val;
	DIR_ENTRY_PAGE temppage;

	s_index = tnode->num_entries;
	if (tnode->child_page_pos[s_index] == 0) {
		/*We are now at the leaf node*/
		/* Just delete and return. Won't need to handle
			underflow here */
		memcpy(extracted_child, &(tnode->dir_entries[s_index-1]),
							sizeof(DIR_ENTRY));
		tnode->num_entries--;

		PWRITE(fh, tnode, sizeof(DIR_ENTRY_PAGE),
							tnode->this_page_pos);
		return 0;
	}

	/* Conduct rebalancing all the way down */

	ret_val = rebalance_btree(tnode, fh, this_meta, s_index, tmp_entries,
			temp_child_page_pos, meta_pos);

	/* If rebalanced, recheck by calling this function with the same
		parameters, else read the child node and go down the tree */
	if (ret_val > 0) {
		if (ret_val == 2) {
			/* Need to reload tnode. Old one is deleted */
			PREAD(fh, &temppage, sizeof(DIR_ENTRY_PAGE),
						this_meta->root_entry_page);
			return extract_largest_child(&temppage, fh, this_meta,
						extracted_child, tmp_entries,
						temp_child_page_pos, meta_pos);
		} else {
			return extract_largest_child(tnode, fh, this_meta,
						extracted_child, tmp_entries,
						temp_child_page_pos, meta_pos);
		}
	}

	if (ret_val < 0)
		return ret_val;

	PREAD(fh, &temppage, sizeof(DIR_ENTRY_PAGE),
						tnode->child_page_pos[s_index]);

	return extract_largest_child(&temppage, fh, this_meta,
			extracted_child, tmp_entries, temp_child_page_pos,
			meta_pos);

errcode_handle:
	return errcode;
}


/************************************************************************r
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dedup_table.c
* Abstract: The c source code file for data deduplication table.
*
* Revision History
* 2015/07/17 Yuxun create this file
*
**************************************************************************/
#include "dedup_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <openssl/sha.h>

#include "global.h"
#include "macro.h"
#include "logger.h"


// Mock function - to ignore logger
int write_log(int level, char *format, ...) {
    return 0;
}


int initialize_ddt_meta() {

	FILE *fptr;
	int fd;
	char *ddt_meta_file = "ddt_meta_0";
	DDT_BTREE_META ddt_meta;
	int errcode;
	ssize_t ret_ssize;


	fptr = fopen(ddt_meta_file, "w+");
	fd = fileno(fptr);

	// Init meta struct
	memset(&ddt_meta, 0, sizeof(DDT_BTREE_META));
	ddt_meta.tree_root = 0;
	ddt_meta.total_el = 0;
	ddt_meta.node_gc_list = 0;

	PWRITE(fd, &ddt_meta, sizeof(DDT_BTREE_META), 0);

	return fd;

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: search_ddt_btree
*        Inputs: unsigned char *key, DDT_BTREE_NODE *tnode,
*                int fd, DDT_BTREE_NODE *result_node, int *result_idx
*       Summary: Search a element by a given hash key. The search operation
*                starts with tnode and go deeper until the element is found
*                or there are no nodes to search. Return the tree node
*                (result_node) which contains the key element and the index
*                (result_index) of elements array in the node.
*  Return value: 0 if the element is found, or -1 if not.
*
*************************************************************************/
int search_ddt_btree(unsigned char *key, DDT_BTREE_NODE *tnode, int fd,
				DDT_BTREE_NODE *result_node, int *result_idx){

	int search_idx;
	int cmpare_result;
	DDT_BTREE_NODE temp_node;
	int ret_val;
	int errcode;
	ssize_t ret_ssize;

	for (search_idx = 0; search_idx < tnode->num_el; search_idx++) {
		cmpare_result = memcmp(key, tnode->ddt_btree_el[search_idx].obj_id,
 					   	 //SHA256_DIGEST_LENGTH);
 					   	 4);
		if (cmpare_result == 0) {
			memcpy(result_node, tnode, sizeof(DDT_BTREE_NODE));
			*result_idx = search_idx;
			return 0;
		} else if (cmpare_result < 0) {
			break;
		}
	}

	// this node is leaf
	if (tnode->is_leaf) {
		printf("%s", "Key doesn't existed");
		return -1;
	}

	// Go deeper to search
	memset(&temp_node, 0, sizeof(DDT_BTREE_NODE));
	PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
					tnode->child_node_pos[search_idx]);

	return search_ddt_btree(key, &temp_node, fd, result_node, result_idx);

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: traverse_ddt_btree
*        Inputs: DDT_BTREE_NODE *tnode, int fd
*       Summary: Travese btree. Starts from tnode and list all keys
*                in ascending order.
*  Return value: 0
*
*************************************************************************/
int traverse_ddt_btree(DDT_BTREE_NODE *tnode, int fd) {

	DDT_BTREE_NODE temp_node;
	int errcode;
	ssize_t ret_ssize;

	memset(&temp_node, 0, sizeof(DDT_BTREE_NODE));

	// this node is leaf
	if (tnode->is_leaf) {
		for (int i = 0; i < tnode->num_el; i++) {
		   	printf("In leaf - %s\n", tnode->ddt_btree_el[i].obj_id);
		}
		return 0;
	}

	for (int i = 0; i < tnode->num_el + 1; i++) {
		if (!tnode->is_leaf) {
			PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE), tnode->child_node_pos[i]);
			traverse_ddt_btree(&temp_node, fd);
		}
		if (i < tnode->num_el) {
			printf("Internal - %s\n", tnode->ddt_btree_el[i].obj_id);
		}
	}

	return 0;

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: insert_ddt_btree
*        Inputs: unsigned char *key, DDT_BTREE_NODE *tnode, int fd,
*                DDT_BTREE_META *this_meta
*       Summary: Insert a element with given hash key. Insert operation
*                begins with tnode and goes deeper until a leaf node is
*                found. Will split the child node if encountered a full
*                child.
*  Return value: 0 if the element is found, or -1 if not.
*
*************************************************************************/
int insert_ddt_btree(unsigned char *key, DDT_BTREE_NODE *tnode, int fd, DDT_BTREE_META *this_meta) {

	DDT_BTREE_EL new_el;
	DDT_BTREE_NODE new_root;
	DDT_BTREE_NODE temp_node;
	int errcode;
	off_t ret_pos;
	ssize_t ret_ssize;

	// Init new elements
	memset(&new_el, 0, sizeof(DDT_BTREE_EL));
	memcpy(new_el.obj_id, key, SHA256_DIGEST_LENGTH);
	new_el.obj_size = 0;
	new_el.refcount = 1;

	if (this_meta->tree_root == 0) {
		// The first element, need to create root node
		new_root.num_el = 1;
		new_root.is_leaf = TRUE;
		new_root.parent_node_pos = 0;
		new_root.gc_list_next = 0;
		new_root.ddt_btree_el[0] = new_el;

		LSEEK(fd, 0, SEEK_END);
		new_root.this_node_pos = ret_pos;
		// Write new node to disk
		PWRITE(fd, &new_root, sizeof(DDT_BTREE_EL), new_root.this_node_pos);

		// Update btree meta and write to disk
		this_meta->total_el += 1;
		this_meta->tree_root = new_root.this_node_pos;

		// assign new root
		memcpy(tnode, &new_root, sizeof(DDT_BTREE_NODE));

		printf("new root pos - %lld\n", new_root.this_node_pos);

	} else if (tnode->num_el == MAX_EL_PER_NODE) {
		// Detect full root, need to create new root
		// Copy old root
		memset(&temp_node, 0, sizeof(DDT_BTREE_NODE));
		memcpy(&temp_node, tnode, sizeof(DDT_BTREE_NODE));

		memset(&new_root, 0, sizeof(DDT_BTREE_NODE));
		new_root.num_el = 0;
		new_root.is_leaf = FALSE;
		new_root.parent_node_pos = 0;
		new_root.gc_list_next = 0;
		new_root.child_node_pos[0] = temp_node.this_node_pos;

		if (this_meta->node_gc_list != 0) {
			// We can reclaim a node
			PREAD(fd, &new_root, sizeof(DDT_BTREE_NODE), this_meta->node_gc_list);
			new_root.gc_list_next = this_meta->node_gc_list;
			this_meta->node_gc_list = new_root.gc_list_next;
		} else {
			LSEEK(fd, 0, SEEK_END);
			new_root.this_node_pos = ret_pos;
		}

		// Write new node to disk
		PWRITE(fd, &new_root, sizeof(DDT_BTREE_NODE), new_root.this_node_pos);

		// Split child
		_split_child_ddt_btree(&new_root, 0, &temp_node, fd, this_meta);
		// Insert element with new root
		_insert_non_full_ddt_btree(&new_el, &new_root, fd, this_meta);

		// Root is changed - Update btree meta and write to disk
		this_meta->total_el += 1;
		this_meta->tree_root = new_root.this_node_pos;
		printf("new root pos - %lld\n", new_root.this_node_pos);

		// assign new root
		memcpy(tnode, &new_root, sizeof(DDT_BTREE_NODE));

	} else {
		// Just insert nonfull
		_insert_non_full_ddt_btree(&new_el, tnode, fd, this_meta);

		this_meta->total_el += 1;
		PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);
	}

	// Btree meta is changed - update it
	PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);

	return 0;

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: _insert_non_full_ddt_btree
*        Inputs: DDT_BTREE_EL *new_element, DDT_BTREE_NODE *tnode, int fd,
*                DDT_BTREE_META *this_meta
*       Summary: Insert a element to non-full tree. Find the leaf node and
*                insert the element to it. Will split the child node if
*                encountered a full node before arrived leaf.
*  Return value: 0 if the element is found, or -1 if not.
*
*************************************************************************/
int _insert_non_full_ddt_btree(DDT_BTREE_EL *new_element, DDT_BTREE_NODE *tnode,
				int fd, DDT_BTREE_META *this_meta) {

	int search_idx;
	int compare_result;
	DDT_BTREE_NODE temp_node;
    int errcode;
    ssize_t ret_ssize;
	off_t ret_pos;


	// To find which index we should insert or go deeper
	search_idx = 0;
	for (search_idx = 0; search_idx < tnode->num_el; search_idx++) {
		compare_result = memcmp(new_element->obj_id,
						tnode->ddt_btree_el[search_idx].obj_id,
						SHA256_DIGEST_LENGTH);
		if (compare_result < 0) {
			break;
		}
	}

	if (tnode->is_leaf) {
		// This node is leaf
		// Shift all elements which is greater than key in array
		memmove(&(tnode->ddt_btree_el[search_idx+1]), &(tnode->ddt_btree_el[search_idx]),
						(tnode->num_el - search_idx)*sizeof(DDT_BTREE_EL));

		// Insert new element
		tnode->ddt_btree_el[search_idx] = *new_element;
		(tnode->num_el)++;

		// Write to disk after insert
		PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);

	} else {
		// Read the child node for going deeper
		PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE), tnode->child_node_pos[search_idx]);
		if (temp_node.num_el == MAX_EL_PER_NODE) {
			// Need to split child if next target node is full
			_split_child_ddt_btree(tnode, search_idx, &temp_node, fd, this_meta);

			// After split child, we should decide which child to insert new element
			// =>left child or right child
			compare_result = memcmp(new_element->obj_id,
							tnode->ddt_btree_el[search_idx].obj_id,
							SHA256_DIGEST_LENGTH);
			if (compare_result > 0) {
				search_idx++;
				PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE), tnode->child_node_pos[search_idx]);
			}
		}

		_insert_non_full_ddt_btree(new_element, &temp_node, fd, this_meta);
	}


	return 0;

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: _split_child_ddt_btree
*        Inputs: DDT_BTREE_NODE *pnode, int s_idx, DDT_BTREE_NODE *cnode,
*                int fd, DDT_BTREE_META *this_meta
*       Summary: To spilt a full child node (cnode) into two nodes.
*                (s_idx) is used to find the child node (cnode) in parent
*                node (pnode).
*  Return value: 0 if split was successful, or -1 if not.
*
*************************************************************************/
int _split_child_ddt_btree(DDT_BTREE_NODE *pnode, int s_idx, DDT_BTREE_NODE *cnode,
				int fd, DDT_BTREE_META *this_meta) {

	int el_per_child, median;
	DDT_BTREE_NODE new_node;
    int errcode;
    ssize_t ret_ssize;
	off_t ret_pos;


	el_per_child = ((MAX_EL_PER_NODE - 1)/2);
	median = el_per_child + 1;

	// Need a new node
	if (this_meta->node_gc_list != 0) {
		// We can reclaim a node
		PREAD(fd, &new_node, sizeof(DDT_BTREE_NODE), this_meta->node_gc_list);
		new_node.gc_list_next = this_meta->node_gc_list;
		this_meta->node_gc_list = new_node.gc_list_next;
	} else {
		memset(&new_node, 0, sizeof(DDT_BTREE_NODE));
		LSEEK(fd, 0, SEEK_END);
		new_node.this_node_pos = ret_pos;
	}
	new_node.num_el = el_per_child;
	new_node.is_leaf=(cnode->is_leaf)?1:0;
	new_node.parent_node_pos = pnode->this_node_pos;
	new_node.gc_list_next = 0;

	// Copy all elements and children after median
	memcpy(&(new_node.ddt_btree_el[0]), &(cnode->ddt_btree_el[median]),
					el_per_child*sizeof(DDT_BTREE_EL));

	if (!cnode->is_leaf) {
		// Need to copy the pointer to child node for non-leaf node
		memcpy(&(new_node.child_node_pos[0]), &(cnode->child_node_pos[median]),
						(el_per_child + 1)*sizeof(long long));
	}

	// cnode has same number of children too
	cnode->num_el = el_per_child;

	// write new node and cnode to disk
	PWRITE(fd, &new_node, sizeof(DDT_BTREE_NODE), new_node.this_node_pos);
	PWRITE(fd, cnode, sizeof(DDT_BTREE_NODE), cnode->this_node_pos);

	// Need to handle pnode
	// Shift element
	memmove(&(pnode->ddt_btree_el[s_idx+1]), &(pnode->ddt_btree_el[s_idx]),
					(pnode->num_el - s_idx)*sizeof(DDT_BTREE_EL));

	// Shift children
	// We don't need to shift children if pnode is new root
	if (pnode->num_el > 0) {
		memmove(&(pnode->child_node_pos[s_idx+2]), &(pnode->child_node_pos[s_idx+1]),
						(pnode->num_el - s_idx)*sizeof(long long));
	}

	// Update pnode status
	// Move the median element to pnode
	pnode->ddt_btree_el[s_idx] = cnode->ddt_btree_el[el_per_child];
	pnode->child_node_pos[s_idx+1] = new_node.this_node_pos;
	(pnode->num_el)++;
	// Write pnode to disk
	PWRITE(fd, pnode, sizeof(DDT_BTREE_NODE), pnode->this_node_pos);

	return 0;

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: delete_ddt_btree
*        Inputs: unsigned char *key, DDT_BTREE_NODE *tnode, int fd,
*                DDT_BTREE_META *this_meta
*       Summary: To find a element with a given key and delete it.
*  Return value: 0 if operation was successful, or -1 if not.
*
*************************************************************************/
int delete_ddt_btree(unsigned char *key, DDT_BTREE_NODE *tnode,
				int fd, DDT_BTREE_META *this_meta) {

	int search_idx;
	int compare_result, match;
	DDT_BTREE_NODE temp_node, largest_child_node;
	DDT_BTREE_EL largest_child_el;
    int errcode;
    ssize_t ret_ssize;

	// Init status
	match = FALSE;

	for (search_idx = 0; search_idx < tnode->num_el; search_idx++) {
		compare_result = memcmp(key, tnode->ddt_btree_el[search_idx].obj_id,
						//SHA256_DIGEST_LENGTH);
						4);
		if (compare_result == 0) {
			match = TRUE;
			break;
		} else if (compare_result < 0) {
			break;
		}
	}

	if (!match) {
		if (!tnode->is_leaf) {
			PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE), tnode->child_node_pos[search_idx]);
			delete_ddt_btree(key, &temp_node, fd, this_meta);
		} else {
			// Can't find the element to be deleted, return error
			printf("No matched key\n");
			return -1;
		}
	} else {
		if (tnode->is_leaf) {
			// Element in leaf - just remove it
			if (search_idx < (tnode->num_el -1)) {
				// No need to shift elements for the largest element in this node
				memmove(&(tnode->ddt_btree_el[search_idx]), &(tnode->ddt_btree_el[search_idx+1]),
								(tnode->num_el - search_idx - 1)*sizeof(DDT_BTREE_EL));
			}
			tnode->num_el--;
			// Update tnode in disk
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);

		} else {
			// Elements to be deleted is in an internal node.
			// We need to replace the element with the largest child in left-subtree.
			// After replacement, remove the the largest child in leaf node.
			memset(&largest_child_node, 0, sizeof(DDT_BTREE_NODE));
			memset(&largest_child_el, 0, sizeof(DDT_BTREE_EL));

			// Find the largest child
			PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE), tnode->child_node_pos[search_idx]);
			_extract_largest_child(&temp_node, fd, &largest_child_node,
					&largest_child_el, this_meta);
			// Copy the value of largest child
			memcpy(&(tnode->ddt_btree_el[search_idx]), &largest_child_el, sizeof(DDT_BTREE_EL));

			// Update this node
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);
		}
	}

	// Attempt to rebalance
	_rebalance_btree(tnode, search_idx, fd, this_meta);

	return 0;

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: _extract_largest_child
*        Inputs: DDT_BTREE_NODE *tnode, int fd, DDT_BTREE_NODE *result_node,
*                DDT_BTREE_EL *result_el, DDT_BTREE_META *this_meta
*       Summary: To find a largest child of tnode and remove it. Return the
*                node(result_node) and element(result_el) where the child is
*                original in.
*  Return value: 0 if operation was successful, or -1 if not.
*
*************************************************************************/
int _extract_largest_child(DDT_BTREE_NODE *tnode, int fd, DDT_BTREE_NODE *result_node,
			DDT_BTREE_EL *result_el, DDT_BTREE_META *this_meta) {
	// To find the largest child for tnode

	int max_child;
	DDT_BTREE_NODE temp_node;
	int errcode;
	ssize_t ret_ssize;

	max_child = tnode->num_el;

	if (tnode->is_leaf) {
		// Find the largest one, copy the value and return
		memcpy(result_node, tnode, sizeof(DDT_BTREE_NODE));
		memcpy(result_el, &(tnode->ddt_btree_el[max_child-1]), sizeof(DDT_BTREE_EL));
		// Remove largest element in this node
		(tnode->num_el)--;

		// Update to disk
		PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);
		return 0;
	}

	PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
					tnode->child_node_pos[max_child]);
	_extract_largest_child(&temp_node, fd, result_node, result_el, this_meta);

	// Need to rebalance tree because of a element is deleted in this node
	_rebalance_btree(tnode, tnode->num_el, fd, this_meta);

	return 0;

errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: _rebalance_btree
*        Inputs: DDT_BTREE_NODE *tnode, int selected_child, int fd,
*                DDT_BTREE_META *this_meta
*       Summary: Rebalance the tree if there is a node has less than
*                MIN_EL_PER_NODE elements. There two steps of rebalance -
*                rotate and merge.
*                Rotate - To find a sibling node with sufficient elements
*                         and borrow a element from it.
*                Merge - To merge the with a sibling node.
*  Return value: 0 if operation was successful, or -1 if not.
*
*************************************************************************/
int _rebalance_btree(DDT_BTREE_NODE *tnode, int selected_child, int fd,
				DDT_BTREE_META *this_meta) {

	int selected_sibling;
	int num_el_selected_child, num_el_selected_sibling;
	int finish_rotate, finish_merge;
	int change_root;
	DDT_BTREE_NODE child_node, sibling_node;
	int errcode;
	ssize_t ret_ssize;

	// Initialize
	finish_rotate = FALSE;
	finish_merge = FALSE;
	change_root = FALSE;
	memset(&child_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&sibling_node, 0, sizeof(DDT_BTREE_NODE));

	// Got the child node
	PREAD(fd, &child_node, sizeof(DDT_BTREE_NODE),
					tnode->child_node_pos[selected_child]);

	num_el_selected_child = child_node.num_el;

	if (tnode->is_leaf || num_el_selected_child >= MIN_EL_PER_NODE) {
		// Cases that no need to rebalance
		// Leaf node or node has enough elements
		return 0;
	}

	if (selected_child < 0 || selected_child > tnode->num_el) {
		// child idx out of bound
		return -1;
	}

	// To find left or right sibling which contain one more el than MIN_EL_PER_NODE
	// To check left sibling
	if (selected_child > 0) {
		// Extract left sibling
		PREAD(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
				tnode->child_node_pos[selected_child-1]);

		if (sibling_node.num_el > MIN_EL_PER_NODE) {
			printf("Start rotete right\n");
			// Left sibling is selected, start to rotate right
			selected_sibling = selected_child - 1;
			num_el_selected_sibling = sibling_node.num_el;

			// Move seperator to selected child, and selected sibling to selected child
			memmove(&(child_node.ddt_btree_el[1]),
							&(child_node.ddt_btree_el[0]),
							num_el_selected_child*sizeof(DDT_BTREE_EL));

			memcpy(&(child_node.ddt_btree_el[0]),
							&(tnode->ddt_btree_el[selected_child-1]),
							sizeof(DDT_BTREE_EL));

			memcpy(&(tnode->ddt_btree_el[selected_child-1]),
							&(sibling_node.ddt_btree_el[num_el_selected_sibling-1]),
							sizeof(DDT_BTREE_EL));

			// Need to rotate children too
			if (!child_node.is_leaf) {
				memmove(&(child_node.child_node_pos[1]),
								&(child_node.child_node_pos[0]),
								(num_el_selected_child)*sizeof(long long));

				memcpy(&(child_node.child_node_pos[0]),
								&(sibling_node.child_node_pos[num_el_selected_sibling]),
								sizeof(long long));
			}

			// Update node status
			(child_node.num_el)++;
			(sibling_node.num_el)--;

			// Three nodes changed - Update in disk
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);
			PWRITE(fd, &child_node, sizeof(DDT_BTREE_NODE), child_node.this_node_pos);
			PWRITE(fd, &sibling_node, sizeof(DDT_BTREE_NODE), sibling_node.this_node_pos);

			// Rotate done
			finish_rotate = TRUE;
		}
	}

	// To check right sibling
	if (!finish_rotate && selected_child < tnode->num_el) {
		// Extract right sibling
		PREAD(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
				tnode->child_node_pos[selected_child+1]);

		if (sibling_node.num_el > MIN_EL_PER_NODE) {
			printf("Start rotete left\n");
			// Right sibling is selected, start to rotate left
			selected_sibling = selected_child + 1;
			num_el_selected_sibling = sibling_node.num_el;

			// Move seperator to selected child, and selected sibling to selected child
			memcpy(&(child_node.ddt_btree_el[num_el_selected_child]),
							&(tnode->ddt_btree_el[selected_child]), sizeof(DDT_BTREE_EL));

			memcpy(&(tnode->ddt_btree_el[selected_child]),
							&(sibling_node.ddt_btree_el[0]),
							sizeof(DDT_BTREE_EL));

			memmove(&(sibling_node.ddt_btree_el[0]),
							&(sibling_node.ddt_btree_el[1]),
							(num_el_selected_sibling-1)*sizeof(DDT_BTREE_EL));

			// Need to rotate children too
			if (!child_node.is_leaf) {
				memcpy(&(child_node.child_node_pos[num_el_selected_child+1]),
								&(sibling_node.child_node_pos[0]),
								sizeof(long long));

				memmove(&(sibling_node.child_node_pos[0]),
								&(sibling_node.child_node_pos[1]),
								(num_el_selected_sibling)*sizeof(long long));
			}

			// Update number of elements
			(child_node.num_el)++;
			(sibling_node.num_el)--;

			// Three nodes changed - Update in disk
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);
			PWRITE(fd, &child_node, sizeof(DDT_BTREE_NODE), child_node.this_node_pos);
			PWRITE(fd, &sibling_node, sizeof(DDT_BTREE_NODE), sibling_node.this_node_pos);

			// Rotate done
			finish_rotate = TRUE;
		}
	}

	/* Cannot find a sibling to rotate, start to merge.

	   **Note**
	   We always merge the right node to left in merge process,
	   so the meaning of each node is changed below.
	       1. sibling_node always represents the left node
	       2. child_node always represents the right node
	*/

	if (!finish_rotate) {
		printf("Start merge\n");
		if (selected_child <= 0) {
			// Merge with right sibling
			selected_sibling = selected_child;

		} else {
			// Merge with left sibling
			selected_sibling = selected_child - 1;
		}


		PREAD(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
				tnode->child_node_pos[selected_sibling]);

		PREAD(fd, &child_node, sizeof(DDT_BTREE_NODE),
				tnode->child_node_pos[selected_sibling+1]);

		num_el_selected_sibling = sibling_node.num_el;
		num_el_selected_child = child_node.num_el;

		// Copy parent and all elements in right node to left
		memcpy(&(sibling_node.ddt_btree_el[num_el_selected_sibling]),
						&(tnode->ddt_btree_el[selected_sibling]),
						sizeof(DDT_BTREE_EL));

		memcpy(&(sibling_node.ddt_btree_el[num_el_selected_sibling+1]),
						&(child_node.ddt_btree_el[0]),
						num_el_selected_child*sizeof(DDT_BTREE_EL));

		if (!sibling_node.is_leaf) {
			memcpy(&(sibling_node.child_node_pos[num_el_selected_sibling+1]),
							&(child_node.child_node_pos[0]),
							(num_el_selected_child+1)*sizeof(long long));
		}

		// Reclaim child node
		child_node.gc_list_next = this_meta->node_gc_list;
		this_meta->node_gc_list = child_node.this_node_pos;

		// An element is removed from parent node.
		// Should handle both elements and children in parent node.
		if (tnode->num_el > 1) {
			memmove(&(tnode->ddt_btree_el[selected_sibling]),
							&(tnode->ddt_btree_el[selected_sibling+1]),
							(tnode->num_el - selected_sibling - 1)*sizeof(DDT_BTREE_EL));

			memmove(&(tnode->child_node_pos[selected_sibling+1]),
							&(tnode->child_node_pos[selected_sibling+2]),
							(tnode->num_el - selected_sibling -1)*sizeof(long long));
		} else if (tnode->parent_node_pos == 0) {
			printf("Reclaim root\n");
			// Tree root has no elements anymore, reclaim it
			tnode->gc_list_next = this_meta->node_gc_list;
			this_meta->node_gc_list = tnode->this_node_pos;
			this_meta->tree_root = sibling_node.this_node_pos;

			change_root = TRUE;
		}

		(tnode->num_el)--;
		sibling_node.num_el += (num_el_selected_child + 1);

		// Three nodes changed - Update in disk
		PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);
		PWRITE(fd, &child_node, sizeof(DDT_BTREE_NODE), child_node.this_node_pos);
		PWRITE(fd, &sibling_node, sizeof(DDT_BTREE_NODE), sibling_node.this_node_pos);

		// Update btree meta
		PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);

		if (change_root) {
			// assign new root
			memcpy(tnode, &sibling_node, sizeof(DDT_BTREE_NODE));
		}

		// Merge done
		finish_merge = TRUE;
	}

	return 0;

errcode_handle:
	return errcode;
}

// Main function to test
int main() {

	int fd;
	DDT_BTREE_META this_meta;
	DDT_BTREE_NODE tree_root;
	int errcode;
	ssize_t ret_ssize;

	// init test data
	unsigned char testdata0[] = "0000";
	unsigned char testdata1[] = "1111";
	unsigned char testdata2[] = "2222";
	unsigned char testdata3[] = "3333";
	unsigned char testdata4[] = "4444";
	unsigned char testdata5[] = "5555";
	unsigned char testdata6[] = "6666";
	unsigned char testdata7[] = "7777";
	unsigned char testdata8[] = "8888";
	unsigned char testdata9[] = "9999";

	// Initialize table
	fd = initialize_ddt_meta();

	PREAD(fd, &this_meta, sizeof(DDT_BTREE_META), 0);

	memset(&tree_root, 0, sizeof(DDT_BTREE_NODE));

	insert_ddt_btree(testdata0, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata1, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata2, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata3, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata4, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata5, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata6, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata7, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata8, &tree_root, fd, &this_meta);
	insert_ddt_btree(testdata9, &tree_root, fd, &this_meta);

	delete_ddt_btree(testdata3, &tree_root, fd, &this_meta);
	delete_ddt_btree(testdata4, &tree_root, fd, &this_meta);
	delete_ddt_btree(testdata6, &tree_root, fd, &this_meta);
	delete_ddt_btree(testdata7, &tree_root, fd, &this_meta);

	traverse_ddt_btree(&tree_root, fd);

	return 0;

errcode_handle:
	return errcode;
}

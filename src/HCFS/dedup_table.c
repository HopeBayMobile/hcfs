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
#include <sys/file.h>
#include <sys/stat.h>

#include "global.h"
#include "macro.h"
#include "logger.h"
#include "utils.h"

#if (DEDUP_ENABLE)
int32_t initialize_ddt_meta(char *meta_path)
{

	FILE *fptr;
	int32_t fd;
	DDT_BTREE_META ddt_meta;
	int32_t errcode;
	ssize_t ret_ssize;

	fptr = fopen(meta_path, "wb");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
				strerror(errcode));

		errcode = -errcode;
		goto errcode_handle;
	}
	fd = fileno(fptr);
	flock(fd, LOCK_EX);
	setbuf(fptr, NULL);

	/* Init meta struct */
	memset(&ddt_meta, 0, sizeof(DDT_BTREE_META));
	ddt_meta.tree_root = 0;
	ddt_meta.total_el = 0;
	ddt_meta.node_gc_list = 0;

	PWRITE(fd, &ddt_meta, sizeof(DDT_BTREE_META), 0);

	flock(fd, LOCK_UN);
	fclose(fptr);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: get_btree_meta
*        Inputs: uint8_t key[], DDT_BTREE_NODE *root,
*                DDT_BTREE_META this_meta
*       Summary: Get point to the metadata of a tree. Also return the root node
*                (root) and the metadata(this_meta) of this tree. The meta file
*                will be locked by flock after this oepration.
*  Return value: File pointer to btree file
*
*************************************************************************/
FILE *get_ddt_btree_meta(uint8_t key[], DDT_BTREE_NODE *root,
			 DDT_BTREE_META *this_meta)
{

	char meta_path[1000];
	FILE *fptr;
	int32_t fd;
	int32_t ret;
	int32_t errcode;
	ssize_t ret_ssize;

	/* Get metafile name by the first char of hash key */
	ret = fetch_ddt_path(meta_path, key[0]);
	if (ret < 0) {
		/* Get the metafile path failed */
		goto errcode_handle;
	}

	/* Initialize tree if not existed */
	if (access(meta_path, F_OK | W_OK) < 0) {
		ret = initialize_ddt_meta(meta_path);
		if (ret < 0)
			goto errcode_handle;
	}

	/* Open file */
	fptr = fopen(meta_path, "r+b");
	if (fptr == NULL) {
		errcode = errno;
		write_log(0, "IO error in %s. Code %d, %s\n", __func__, errcode,
				strerror(errcode));
		goto errcode_handle;
	}
	fd = fileno(fptr);
	flock(fd, LOCK_EX);
	setbuf(fptr, NULL);

	/* Copy metadata */
	PREAD(fd, this_meta, sizeof(DDT_BTREE_META), 0);
	if (this_meta->tree_root > 0)
		PREAD(fd, root, sizeof(DDT_BTREE_NODE), this_meta->tree_root);

	return fptr;

errcode_handle:
	return NULL;
}

/************************************************************************
*
* Function name: search_ddt_btree
*        Inputs: uint8_t key[], DDT_BTREE_NODE *tnode,
*                int32_t fd, DDT_BTREE_NODE *result_node, int32_t *result_idx
*       Summary: Search a element by a given hash key. The search operation
*                starts with tnode and go deeper until the element is found
*                or there are no nodes to search. Return the tree node
*                (result_node) which contains the key element and the index
*                (result_index) of elements array in the node.
*  Return value: 0 if the element is found
*                1 if element not found
*                -1 if error occured.
*
*************************************************************************/
int32_t search_ddt_btree(uint8_t key[], DDT_BTREE_NODE *tnode, int32_t fd,
		     DDT_BTREE_NODE *result_node, int32_t *result_idx)
{

	int32_t search_idx;
	int32_t compare_result;
	DDT_BTREE_NODE temp_node;
	DDT_BTREE_EL temp_el;
	int32_t errcode;
	ssize_t ret_ssize;

	if (tnode->num_el == 0) {
		/* Search on empty node */
		return -1;
	}

	for (search_idx = 0; search_idx < tnode->num_el; search_idx++) {
		memcpy(&temp_el, &(tnode->ddt_btree_el[search_idx]),
		       sizeof(DDT_BTREE_EL));
		compare_result =
		    memcmp(key, temp_el.obj_id, OBJID_LENGTH);

		if (compare_result == 0) {
			memcpy(result_node, tnode, sizeof(DDT_BTREE_NODE));
			*result_idx = search_idx;
			return 0;
		} else if (compare_result < 0) {
			break;
		}
	}

	/* this node is leaf */
	if (tnode->is_leaf) {
		//printf("Key - %02x doesn't existed\n", key[0]);
		return 1;
	}

	/*Go deeper to search */
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
*        Inputs: DDT_BTREE_NODE *tnode, int32_t fd
*       Summary: Travese btree. Starts from tnode and list all keys
*                in ascending order.
*  Return value: 0
*
*************************************************************************/
int32_t traverse_ddt_btree(DDT_BTREE_NODE *tnode, int32_t fd)
{

	int32_t search_idx, tmp_idx;
	DDT_BTREE_NODE temp_node;
	int32_t errcode;
	ssize_t ret_ssize;

	if (tnode->is_leaf) {
		for (search_idx = 0; search_idx < tnode->num_el; search_idx++) {
			printf("Leaf - ");
			for (tmp_idx = 0; tmp_idx < OBJID_LENGTH;
			     ++tmp_idx)
				printf("%02x", tnode->ddt_btree_el[search_idx]
						   .obj_id[tmp_idx]);
			printf(" with refcount (%" PRId64 ") in node - %" PRId64 "\n",
			       tnode->ddt_btree_el[search_idx].refcount,
			       tnode->this_node_pos);
		}
		return 0;
	}

	for (search_idx = 0; search_idx < tnode->num_el + 1; search_idx++) {
		if (!tnode->is_leaf) {
			memset(&temp_node, 0, sizeof(DDT_BTREE_NODE));
			PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
			      tnode->child_node_pos[search_idx]);
			traverse_ddt_btree(&temp_node, fd);
		}
		if (search_idx < tnode->num_el) {
			printf("Node - ");
			for (tmp_idx = 0; tmp_idx < OBJID_LENGTH;
			     ++tmp_idx)
				printf("%02x", tnode->ddt_btree_el[search_idx]
						   .obj_id[tmp_idx]);
			printf(" with refcount (%" PRId64 ") in node - %" PRId64 "\n",
			       tnode->ddt_btree_el[search_idx].refcount,
			       tnode->this_node_pos);
		}
	}

	return 0;

errcode_handle:
	return errcode;
}

/* static functions for insert_ddt_btree */
static int32_t _insert_non_full_ddt_btree(DDT_BTREE_EL *new_element,
				      DDT_BTREE_NODE *tnode, int32_t fd,
				      DDT_BTREE_META *this_meta);

static int32_t _split_child_ddt_btree(DDT_BTREE_NODE *pnode, int32_t s_idx,
				  DDT_BTREE_NODE *cnode, int32_t fd,
				  DDT_BTREE_META *this_meta);

/************************************************************************
*
* Function name: insert_ddt_btree
*        Inputs: uint8_t key[], DDT_BTREE_NODE *tnode, int32_t fd,
*                DDT_BTREE_META *this_meta
*       Summary: Insert a element with given hash key. Insert operation
*                begins with tnode and goes deeper until a leaf node is
*                found. Will split the child node if encountered a full
*                child.
*  Return value: 0 if the element is found, or -1 if not.
*
*************************************************************************/
int32_t insert_ddt_btree(uint8_t key[], const off_t obj_size,
		     DDT_BTREE_NODE *tnode, int32_t fd,
		     DDT_BTREE_META *this_meta)
{

	DDT_BTREE_EL new_el;
	DDT_BTREE_NODE new_root;
	DDT_BTREE_NODE temp_node;
	int32_t errcode;
	off_t ret_pos;
	ssize_t ret_ssize;

	/* Init new elements */
	memset(&new_el, 0, sizeof(DDT_BTREE_EL));
	memcpy(new_el.obj_id, key, OBJID_LENGTH);
	//memcpy(new_el.start_bytes, start_bytes, BYTES_TO_CHECK);
	//memcpy(new_el.end_bytes, end_bytes, BYTES_TO_CHECK);
	new_el.obj_size = obj_size;
	new_el.refcount = 1;

	if (this_meta->tree_root == 0) {
		/* The first element, need to create root node */
		new_root.num_el = 1;
		new_root.is_leaf = TRUE;
		new_root.parent_node_pos = 0;
		new_root.gc_list_next = 0;
		new_root.ddt_btree_el[0] = new_el;

		LSEEK(fd, 0, SEEK_END);
		new_root.this_node_pos = ret_pos;
		/* Write new node to disk */
		PWRITE(fd, &new_root, sizeof(DDT_BTREE_NODE),
		       new_root.this_node_pos);

		/* Update btree meta and write to disk */
		this_meta->total_el += 1;
		this_meta->tree_root = new_root.this_node_pos;

		/* assign new root */
		memcpy(tnode, &new_root, sizeof(DDT_BTREE_NODE));

	} else if (tnode->num_el >= MAX_EL_PER_NODE) {
		/* Detect full root, need to create new root */
		memset(&temp_node, 0, sizeof(DDT_BTREE_NODE));
		memcpy(&temp_node, tnode, sizeof(DDT_BTREE_NODE));

		memset(&new_root, 0, sizeof(DDT_BTREE_NODE));
		new_root.num_el = 0;
		new_root.is_leaf = FALSE;
		new_root.parent_node_pos = 0;
		new_root.gc_list_next = 0;
		new_root.child_node_pos[0] = temp_node.this_node_pos;

		if (this_meta->node_gc_list != 0) {
			/* Can reclaim a node */
			PREAD(fd, &new_root, sizeof(DDT_BTREE_NODE),
			      this_meta->node_gc_list);
			new_root.gc_list_next = this_meta->node_gc_list;
			this_meta->node_gc_list = new_root.gc_list_next;
		} else {
			LSEEK(fd, 0, SEEK_END);
			new_root.this_node_pos = ret_pos;
		}

		/* Write new node to disk */
		PWRITE(fd, &new_root, sizeof(DDT_BTREE_NODE),
		       new_root.this_node_pos);

		/* Split child */
		_split_child_ddt_btree(&new_root, 0, &temp_node, fd, this_meta);
		/* Insert element with new root */
		_insert_non_full_ddt_btree(&new_el, &new_root, fd, this_meta);

		/* Root is changed - Update btree meta and write to disk */
		this_meta->total_el += 1;
		this_meta->tree_root = new_root.this_node_pos;

		/* assign new root */
		memcpy(tnode, &new_root, sizeof(DDT_BTREE_NODE));

	} else {
		/* Just insert nonfull */
		_insert_non_full_ddt_btree(&new_el, tnode, fd, this_meta);

		this_meta->total_el += 1;
		PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);
	}

	/* Btree meta is changed - update it */
	PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: _insert_non_full_ddt_btree
*        Inputs: DDT_BTREE_EL *new_element, DDT_BTREE_NODE *tnode, int32_t fd,
*                DDT_BTREE_META *this_meta
*       Summary: Insert a element to non-full tree. Find the leaf node and
*                insert the element to it. Will split the child node if
*                encountered a full node before arrived leaf.
*  Return value: 0 if the element is found, or -1 if not.
*
*************************************************************************/
static int32_t _insert_non_full_ddt_btree(DDT_BTREE_EL *new_element,
				      DDT_BTREE_NODE *tnode, int32_t fd,
				      DDT_BTREE_META *this_meta)
{

	int32_t search_idx;
	int32_t compare_result;
	DDT_BTREE_NODE temp_node;
	int32_t errcode;
	ssize_t ret_ssize;

	/* To find which index we should insert or go deeper */
	search_idx = 0;
	for (search_idx = 0; search_idx < tnode->num_el; search_idx++) {
		compare_result = memcmp(new_element->obj_id,
					tnode->ddt_btree_el[search_idx].obj_id,
					OBJID_LENGTH);
		if (compare_result < 0) {
			break;
		}
	}

	if (tnode->is_leaf) {
		/* This node is leaf
		 * Shift all elements which is greater than key in array
		 */
		memmove(&(tnode->ddt_btree_el[search_idx + 1]),
			&(tnode->ddt_btree_el[search_idx]),
			(tnode->num_el - search_idx) * sizeof(DDT_BTREE_EL));

		/* Insert new element */
		tnode->ddt_btree_el[search_idx] = *new_element;
		(tnode->num_el)++;

		/* Write to disk after insert */
		PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);

	} else {
		/* Read the child node for going deeper */
		PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
		      tnode->child_node_pos[search_idx]);
		if (temp_node.num_el == MAX_EL_PER_NODE) {
			/* Need to split child if next target node is full */
			_split_child_ddt_btree(tnode, search_idx, &temp_node,
					       fd, this_meta);

			/* After split child, we should decide which child to
			 * insert new element
			 * =>left child or right child
			 */
			compare_result =
			    memcmp(new_element->obj_id,
				   tnode->ddt_btree_el[search_idx].obj_id,
				   OBJID_LENGTH);
			if (compare_result > 0) {
				search_idx++;
				PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
				      tnode->child_node_pos[search_idx]);
			}
		}

		_insert_non_full_ddt_btree(new_element, &temp_node, fd,
					   this_meta);
	}

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: _split_child_ddt_btree
*        Inputs: DDT_BTREE_NODE *pnode, int32_t s_idx, DDT_BTREE_NODE *cnode,
*                int32_t fd, DDT_BTREE_META *this_meta
*       Summary: To spilt a full child node (cnode) into two nodes.
*                (s_idx) is used to find the child node (cnode) in parent
*                node (pnode).
*  Return value: 0 if split was successful, or -1 if not.
*
*************************************************************************/
static int32_t _split_child_ddt_btree(DDT_BTREE_NODE *pnode, int32_t s_idx,
				  DDT_BTREE_NODE *cnode, int32_t fd,
				  DDT_BTREE_META *this_meta)
{

	int32_t el_left_child, el_right_child, median;
	DDT_BTREE_NODE new_node;
	int32_t errcode;
	ssize_t ret_ssize;
	off_t ret_pos;

	median = cnode->num_el / 2;
	el_left_child = median;
	el_right_child = cnode->num_el - (median + 1);

	/* Need a new node */
	if (this_meta->node_gc_list != 0) {
		/* Can reclaim a node */
		PREAD(fd, &new_node, sizeof(DDT_BTREE_NODE),
		      this_meta->node_gc_list);
		new_node.gc_list_next = this_meta->node_gc_list;
		this_meta->node_gc_list = new_node.gc_list_next;
	} else {
		memset(&new_node, 0, sizeof(DDT_BTREE_NODE));
		LSEEK(fd, 0, SEEK_END);
		new_node.this_node_pos = ret_pos;
	}
	new_node.num_el = el_right_child;
	new_node.is_leaf = (cnode->is_leaf) ? TRUE : FALSE;
	new_node.parent_node_pos = pnode->this_node_pos;
	new_node.gc_list_next = 0;

	/* Copy all elements and children after median */
	memcpy(&(new_node.ddt_btree_el[0]), &(cnode->ddt_btree_el[median + 1]),
	       el_right_child * sizeof(DDT_BTREE_EL));

	if (!cnode->is_leaf) {
		// Need to copy the pointer to child node for non-leaf node
		memcpy(&(new_node.child_node_pos[0]),
		       &(cnode->child_node_pos[median + 1]),
		       (el_right_child + 1) * sizeof(int64_t));
	}

	/* Update cnode stat */
	cnode->num_el = el_left_child;
	cnode->parent_node_pos = new_node.parent_node_pos;

	/* write new node and cnode to disk */
	PWRITE(fd, &new_node, sizeof(DDT_BTREE_NODE), new_node.this_node_pos);
	PWRITE(fd, cnode, sizeof(DDT_BTREE_NODE), cnode->this_node_pos);

	/* Need to handle pnode */
	/* Shift element */
	memmove(&(pnode->ddt_btree_el[s_idx + 1]),
		&(pnode->ddt_btree_el[s_idx]),
		(pnode->num_el - s_idx) * sizeof(DDT_BTREE_EL));

	/* Shift children
	 * We don't need to shift children if pnode is new root
	 */
	if (pnode->num_el > 0) {
		memmove(&(pnode->child_node_pos[s_idx + 2]),
			&(pnode->child_node_pos[s_idx + 1]),
			(pnode->num_el - s_idx) * sizeof(int64_t));
	}

	/* Update pnode status
	 * Move the median element to pnode
	 */
	pnode->ddt_btree_el[s_idx] = cnode->ddt_btree_el[median];
	pnode->child_node_pos[s_idx + 1] = new_node.this_node_pos;
	(pnode->num_el)++;
	/* Write pnode to disk */
	PWRITE(fd, pnode, sizeof(DDT_BTREE_NODE), pnode->this_node_pos);

	return 0;

errcode_handle:
	return errcode;
}

/* static functions for delete_ddt_btee */
static int32_t _extract_largest_child(DDT_BTREE_NODE *tnode, int32_t fd,
				  DDT_BTREE_NODE *result_node,
				  DDT_BTREE_EL *result_el,
				  DDT_BTREE_META *this_meta);

static int32_t _rebalance_btree(DDT_BTREE_NODE *tnode, int32_t selected_child, int32_t fd,
			    DDT_BTREE_META *this_meta);

/************************************************************************
*
* Function name: delete_ddt_btree
*        Inputs: uint8_t key[], DDT_BTREE_NODE *tnode, int32_t fd,
*                DDT_BTREE_META *this_meta, int32_t force_delete
*       Summary: To find a element with a given key. If force_delete is
*                TRUE or the value of refcount decreased is equal to zero,
*                the element will be removed from tree. Otherwise, this
*                operation will only decreased the refcount of element.
*  Return value: 0 if deletion was successful,
*                1 if only decrease the refcount of element,
*                2 if the target element is not found
*                -1 if encountered error.
*
*************************************************************************/
int32_t delete_ddt_btree(uint8_t key[], DDT_BTREE_NODE *tnode, int32_t fd,
		     DDT_BTREE_META *this_meta, int32_t force_delete)
{

	int32_t search_idx;
	int32_t compare_result, match;
	DDT_BTREE_NODE temp_node, largest_child_node;
	DDT_BTREE_EL largest_child_el;
	int32_t ret_val;
	int32_t errcode;
	ssize_t ret_ssize;

	/* Init status */
	match = FALSE;

	if (tnode->num_el <= 0) {
		/* This node doesn't contained any elements */
		printf("This node doesn't contain any elements - node (%" PRId64 ")\n",
		       tnode->this_node_pos);
		return -1;
	}

	for (search_idx = 0; search_idx < tnode->num_el; search_idx++) {
		compare_result =
		    memcmp(key, tnode->ddt_btree_el[search_idx].obj_id,
			   OBJID_LENGTH);
		if (compare_result == 0) {
			match = TRUE;
			break;
		} else if (compare_result < 0) {
			break;
		}
	}

	if (!match) {
		if (!tnode->is_leaf) {
			PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
			      tnode->child_node_pos[search_idx]);
			ret_val = delete_ddt_btree(key, &temp_node, fd,
						   this_meta, force_delete);
			if (ret_val != 0) {
				return ret_val;
			}
		} else {
			/* Can't find the element to be deleted, return error.
			 */
			printf("No matched key - %02x.....%02x\n", key[0],
			       key[31]);
			return 2;
		}
	} else {
		/* Only decrease the refcount, tree rebalancing is not needed.
		 */
		if (!force_delete &&
		    tnode->ddt_btree_el[search_idx].refcount > 1) {
			(tnode->ddt_btree_el[search_idx].refcount)--;
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE),
			       tnode->this_node_pos);
			return 1;
		}

		if (tnode->is_leaf) {
			/* Element in leaf - just remove it */
			if (search_idx < (tnode->num_el - 1)) {
				/* No need to shift elements for the largest
				 * element in this node */
				memmove(&(tnode->ddt_btree_el[search_idx]),
					&(tnode->ddt_btree_el[search_idx + 1]),
					(tnode->num_el - search_idx - 1) *
					    sizeof(DDT_BTREE_EL));
			}
			tnode->num_el--;
			this_meta->total_el--;
			/* Update tnode in disk */
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE),
			       tnode->this_node_pos);
			PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);

		} else {
			/* Elements to be deleted is in an internal node.
			 * We need to replace the element with the largest
			 * child in left-subtree. After replacement, remove
			 * the the largest child in leaf node.
			 */
			memset(&largest_child_node, 0, sizeof(DDT_BTREE_NODE));
			memset(&largest_child_el, 0, sizeof(DDT_BTREE_EL));

			/* Find the largest child */
			PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
			      tnode->child_node_pos[search_idx]);
			_extract_largest_child(&temp_node, fd,
					       &largest_child_node,
					       &largest_child_el, this_meta);
			/* Copy the value of largest child */
			memcpy(&(tnode->ddt_btree_el[search_idx]),
			       &largest_child_el, sizeof(DDT_BTREE_EL));

			this_meta->total_el--;
			/* Update this node and meta */
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE),
			       tnode->this_node_pos);
			PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);
		}
	}

	/* Attempt to rebalance */
	_rebalance_btree(tnode, search_idx, fd, this_meta);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: _extract_largest_child
*        Inputs: DDT_BTREE_NODE *tnode, int32_t fd, DDT_BTREE_NODE *result_node,
*                DDT_BTREE_EL *result_el, DDT_BTREE_META *this_meta
*       Summary: To find a largest child of tnode and remove it. Return the
*                node(result_node) and element(result_el) where the child is
*                original in.
*  Return value: 0 if operation was successful, or -1 if not.
*
*************************************************************************/
static int32_t _extract_largest_child(DDT_BTREE_NODE *tnode, int32_t fd,
				  DDT_BTREE_NODE *result_node,
				  DDT_BTREE_EL *result_el,
				  DDT_BTREE_META *this_meta)
{
	int32_t max_child;
	DDT_BTREE_NODE temp_node;
	int32_t errcode;
	ssize_t ret_ssize;

	max_child = tnode->num_el;

	if (tnode->is_leaf) {
		/* Find the largest one, copy the value and return */
		memcpy(result_node, tnode, sizeof(DDT_BTREE_NODE));
		memcpy(result_el, &(tnode->ddt_btree_el[max_child - 1]),
		       sizeof(DDT_BTREE_EL));
		/* Remove largest element in this node */
		(tnode->num_el)--;

		/* Update to disk */
		PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);
		return 0;
	}

	PREAD(fd, &temp_node, sizeof(DDT_BTREE_NODE),
	      tnode->child_node_pos[max_child]);
	_extract_largest_child(&temp_node, fd, result_node, result_el,
			       this_meta);

	/* Need to rebalance tree because of a element
	 * is deleted
	 */
	_rebalance_btree(tnode, tnode->num_el, fd, this_meta);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: _rebalance_btree
*        Inputs: DDT_BTREE_NODE *tnode, int32_t selected_child, int32_t fd,
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
static int32_t _rebalance_btree(DDT_BTREE_NODE *tnode, int32_t selected_child, int32_t fd,
			    DDT_BTREE_META *this_meta)
{

	int32_t selected_sibling;
	int32_t num_el_selected_child, num_el_selected_sibling;
	int32_t finish_rotate/*, finish_merge*/;
	int32_t change_root;
	DDT_BTREE_NODE child_node, sibling_node;
	int32_t errcode;
	ssize_t ret_ssize;

	/* Initialize */
	finish_rotate = FALSE;
	/* finish_merge = FALSE; */
	change_root = FALSE;
	memset(&child_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&sibling_node, 0, sizeof(DDT_BTREE_NODE));

	/* Got the child node */
	PREAD(fd, &child_node, sizeof(DDT_BTREE_NODE),
	      tnode->child_node_pos[selected_child]);

	num_el_selected_child = child_node.num_el;

	if (tnode->is_leaf || num_el_selected_child >= MIN_EL_PER_NODE) {
		/* Cases that no need to rebalance
		 * Leaf node or node has enough elements
		 */
		return 0;
	}

	if (selected_child < 0 || selected_child > tnode->num_el) {
		/* child idx out of bound */
		return -1;
	}

	/* To find left or right sibling which contain one
	 * more el than MIN_EL_PER_NODE
	 *
	 * To check left sibling
	 */
	if (selected_child > 0) {
		/* Extract left sibling */
		PREAD(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
		      tnode->child_node_pos[selected_child - 1]);

		if (sibling_node.num_el > MIN_EL_PER_NODE) {
			/* Left sibling is selected, start to rotate right */
			selected_sibling = selected_child - 1;
			num_el_selected_sibling = sibling_node.num_el;

			/* Move seperator to selected child,
			 * and selected sibling to selected child.
			 */
			memmove(&(child_node.ddt_btree_el[1]),
				&(child_node.ddt_btree_el[0]),
				num_el_selected_child * sizeof(DDT_BTREE_EL));

			memcpy(&(child_node.ddt_btree_el[0]),
			       &(tnode->ddt_btree_el[selected_child - 1]),
			       sizeof(DDT_BTREE_EL));

			memcpy(
			    &(tnode->ddt_btree_el[selected_child - 1]),
			    &(sibling_node
				  .ddt_btree_el[num_el_selected_sibling - 1]),
			    sizeof(DDT_BTREE_EL));

			/* Need to rotate children too */
			if (!child_node.is_leaf) {
				memmove(&(child_node.child_node_pos[1]),
					&(child_node.child_node_pos[0]),
					(num_el_selected_child) *
					    sizeof(int64_t));

				memcpy(&(child_node.child_node_pos[0]),
				       &(sibling_node.child_node_pos
					     [num_el_selected_sibling]),
				       sizeof(int64_t));
			}

			/* Update node status */
			(child_node.num_el)++;
			(sibling_node.num_el)--;

			/* Three nodes changed - Update in disk */
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE),
			       tnode->this_node_pos);
			PWRITE(fd, &child_node, sizeof(DDT_BTREE_NODE),
			       child_node.this_node_pos);
			PWRITE(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
			       sibling_node.this_node_pos);

			/* Rotate done */
			finish_rotate = TRUE;
		}
	}

	/* To check right sibling */
	if (!finish_rotate && selected_child < tnode->num_el) {
		/* Extract right sibling */
		PREAD(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
		      tnode->child_node_pos[selected_child + 1]);

		if (sibling_node.num_el > MIN_EL_PER_NODE) {
			/* Right sibling is selected, start to rotate left */
			selected_sibling = selected_child + 1;
			num_el_selected_sibling = sibling_node.num_el;

			/* Move seperator to selected child,
			 * and selected sibling to selected child
			 */
			memcpy(
			    &(child_node.ddt_btree_el[num_el_selected_child]),
			    &(tnode->ddt_btree_el[selected_child]),
			    sizeof(DDT_BTREE_EL));

			memcpy(&(tnode->ddt_btree_el[selected_child]),
			       &(sibling_node.ddt_btree_el[0]),
			       sizeof(DDT_BTREE_EL));

			memmove(&(sibling_node.ddt_btree_el[0]),
				&(sibling_node.ddt_btree_el[1]),
				(num_el_selected_sibling - 1) *
				    sizeof(DDT_BTREE_EL));

			/* Need to rotate children too */
			if (!child_node.is_leaf) {
				memcpy(&(child_node.child_node_pos
					     [num_el_selected_child + 1]),
				       &(sibling_node.child_node_pos[0]),
				       sizeof(int64_t));

				memmove(&(sibling_node.child_node_pos[0]),
					&(sibling_node.child_node_pos[1]),
					(num_el_selected_sibling) *
					    sizeof(int64_t));
			}

			/* Update number of elements */
			(child_node.num_el)++;
			(sibling_node.num_el)--;

			/* Three nodes changed - Update in disk */
			PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE),
			       tnode->this_node_pos);
			PWRITE(fd, &child_node, sizeof(DDT_BTREE_NODE),
			       child_node.this_node_pos);
			PWRITE(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
			       sibling_node.this_node_pos);

			/* Rotate done */
			finish_rotate = TRUE;
		}
	}

	/* Cannot find a sibling to rotate, start to merge.
	 *
	 *  **Note**
	 *  We always merge the right node to left in merge process,
	 *  so the meaning of each node is changed below.
	 *      1. sibling_node always represents the left node
	 *      2. child_node always represents the right node
	 */

	if (!finish_rotate) {
		if (selected_child <= 0) {
			/* Merge with right sibling */
			selected_sibling = selected_child;

		} else {
			/* Merge with left sibling */
			selected_sibling = selected_child - 1;
		}

		PREAD(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
		      tnode->child_node_pos[selected_sibling]);

		PREAD(fd, &child_node, sizeof(DDT_BTREE_NODE),
		      tnode->child_node_pos[selected_sibling + 1]);

		num_el_selected_sibling = sibling_node.num_el;
		num_el_selected_child = child_node.num_el;

		/* Copy parent and all elements in right node to left */
		memcpy(&(sibling_node.ddt_btree_el[num_el_selected_sibling]),
		       &(tnode->ddt_btree_el[selected_sibling]),
		       sizeof(DDT_BTREE_EL));

		memcpy(
		    &(sibling_node.ddt_btree_el[num_el_selected_sibling + 1]),
		    &(child_node.ddt_btree_el[0]),
		    num_el_selected_child * sizeof(DDT_BTREE_EL));

		if (!sibling_node.is_leaf) {
			memcpy(
			    &(sibling_node
				  .child_node_pos[num_el_selected_sibling + 1]),
			    &(child_node.child_node_pos[0]),
			    (num_el_selected_child + 1) * sizeof(int64_t));
		}

		/* Reclaim child node */
		child_node.num_el = 0;
		child_node.gc_list_next = this_meta->node_gc_list;
		this_meta->node_gc_list = child_node.this_node_pos;

		/* An element is removed from parent node.
		 * Should handle both elements and children in parent node.
		 */
		if (tnode->num_el > 1) {
			memmove(&(tnode->ddt_btree_el[selected_sibling]),
				&(tnode->ddt_btree_el[selected_sibling + 1]),
				(tnode->num_el - selected_sibling - 1) *
				    sizeof(DDT_BTREE_EL));

			memmove(&(tnode->child_node_pos[selected_sibling + 1]),
				&(tnode->child_node_pos[selected_sibling + 2]),
				(tnode->num_el - selected_sibling - 1) *
				    sizeof(int64_t));
			/* One element is borrowed from tnode */
			(tnode->num_el)--;
		} else {
			if (tnode->parent_node_pos == 0) {
				// printf("Reclaim root - %" PRIu64 "\n",
				// sibling_node.this_node_pos);
				/* Sibling node is new root */
				sibling_node.parent_node_pos = 0;

				/* Has no elements anymore, reclaim it */
				tnode->num_el = 0;
				tnode->gc_list_next = this_meta->node_gc_list;

				this_meta->node_gc_list = tnode->this_node_pos;
				this_meta->tree_root =
				    sibling_node.this_node_pos;

				change_root = TRUE;
			} else {
				/* One element is borrowed from tnode */
				(tnode->num_el)--;
			}
		}

		sibling_node.num_el += (num_el_selected_child + 1);

		/* Three nodes changed - Update in disk */
		PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);
		PWRITE(fd, &child_node, sizeof(DDT_BTREE_NODE),
		       child_node.this_node_pos);
		PWRITE(fd, &sibling_node, sizeof(DDT_BTREE_NODE),
		       sibling_node.this_node_pos);

		/* Update btree meta */
		PWRITE(fd, this_meta, sizeof(DDT_BTREE_META), 0);

		if (change_root) {
			/* Assign new root */
			memcpy(tnode, &sibling_node, sizeof(DDT_BTREE_NODE));
		}

		/* Merge done */
		/* finish_merge = TRUE; */
	}

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: increase_ddt_el_refcount
*        Inputs: DDT_BTREE_NODE *tnode, int32_t s_idx, int32_t fd
*       Summary: Increase the refcount of a specific element
*  Return value: 0 if operation was successful, -1 if not.
*
*************************************************************************/
int32_t increase_ddt_el_refcount(DDT_BTREE_NODE *tnode, int32_t s_idx, int32_t fd)
{

	int32_t errcode;
	ssize_t ret_ssize;

	if (s_idx < 0 || s_idx >= tnode->num_el) {
		printf("Search index out of bound\n");
		return -1;
	}

	/* Increase the refcount */
	tnode->ddt_btree_el[s_idx].refcount += 1;

	/* Update to disk */
	PWRITE(fd, tnode, sizeof(DDT_BTREE_NODE), tnode->this_node_pos);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: decrease_ddt_el_refcount
*        Inputs: uint8_t key[], DDT_BTREE_NODE *tnode, int32_t fd,
*                DDT_BTREE_META *this_meta
*       Summary: Decrease the refcount of element or deleted the emelemt
*                if the refcount is equal to zero.
*  Return value: 0 if element was deleted,
*                1 if only decrease the refcount of element,
*                -1 if encountered error.
*
*************************************************************************/
int32_t decrease_ddt_el_refcount(uint8_t key[], DDT_BTREE_NODE *tnode,
			     int32_t fd, DDT_BTREE_META *this_meta)
{

	return delete_ddt_btree(key, tnode, fd, this_meta, FALSE);
}

int32_t get_obj_id(char *path, uint8_t hash[], uint8_t start_bytes[],
	       uint8_t end_bytes[], off_t *obj_size)
{

	FILE *fptr;
	const int32_t buf_size = 16384;
	char *buf;
	int32_t bytes_read;
	struct stat obj_stat; /* raw file ops */
	off_t size;
	SHA256_CTX ctx;
	int32_t fd;
	int32_t errcode;
	ssize_t ret_ssize;

	if (access(path, R_OK) == -1)
		return -1;

	/* Get file size */
	stat(path, &obj_stat);
	size = obj_stat.st_size;
	*obj_size = size;

	/* Compute hash */
	SHA256_Init(&ctx);
	buf = malloc(buf_size);

	fptr = fopen(path, "rb");
	fd = fileno(fptr);

	while ((bytes_read = fread(buf, 1, buf_size, fptr)))
		SHA256_Update(&ctx, buf, bytes_read);

	SHA256_Final(hash, &ctx);

	/* Copy bytes to check */
	memset(start_bytes, 0, BYTES_TO_CHECK);
	memset(end_bytes, 0, BYTES_TO_CHECK);
	if (size >= BYTES_TO_CHECK) {
		PREAD(fd, start_bytes, BYTES_TO_CHECK, 0);
		size = size - BYTES_TO_CHECK;
		if (size >= BYTES_TO_CHECK) {
			PREAD(fd, end_bytes, BYTES_TO_CHECK, (*obj_size - BYTES_TO_CHECK));
		} else {
			PREAD(fd, end_bytes, size, (*obj_size - size));
		}
	} else {
		PREAD(fd, start_bytes, size, 0);
	}

	fclose(fptr);
	free(buf);

	return 0;

errcode_handle:
	return errcode;
}

int32_t obj_id_to_string(uint8_t obj_id[OBJID_LENGTH],
		   char output_str[OBJID_STRING_LENGTH])
{

	int32_t i;
	for (i = 0; i < OBJID_LENGTH; i++)
		sprintf(output_str + (i * 2), "%02x", obj_id[i]);

	output_str[OBJID_STRING_LENGTH - 1] = 0;
	return 0;
}
#endif /* DEDUP_ENABLE */

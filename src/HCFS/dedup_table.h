/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dedup_table.h
* Abstract: The c header file for data dedup table.
*
* Revision History
* 2015/07/16  Yuxun create this file
*
**************************************************************************/

#include <openssl/sha.h>

#include <stdio.h>

/*
 * The structure of ddt is a combination of hash table and btree. Each node
 * of hash table contains a pointer to the root of a independent btree.
 *
 * To search/insert element from/to ddt, we must choose a node of hash table,
 * by hash function, and then put it to the btree.
 */

// Define the upper/lower bound of btree elements
#define MAX_EL_PER_NODE 3
#define MIN_EL_PER_NODE 1


typedef struct {
	unsigned char obj_id[SHA256_DIGEST_LENGTH];
	long long obj_size;
	long long refcount;
} DDT_BTREE_EL;

typedef struct ddt_btree_node{
	int num_el;
	int is_leaf;
	long long this_node_pos;
	// Elements store in this node
	DDT_BTREE_EL ddt_btree_el[MAX_EL_PER_NODE];
	// Parent node
	long long parent_node_pos;
	// Child nodes
	long long child_node_pos[MAX_EL_PER_NODE+1];
	// Point to next reclaimed node
	long long gc_list_next;
} DDT_BTREE_NODE;

typedef struct {
	long long tree_root;
	long long total_el;
	// Point to the first node which can be reclaimed
	long long node_gc_list;
} DDT_BTREE_META;


int initialize_ddt_meta();

FILE* get_btree_meta(unsigned char *key, DDT_BTREE_NODE *root,
				DDT_BTREE_META *this_meta);

int search_ddt_btree(unsigned char *key, DDT_BTREE_NODE *tnode, int fd,
				DDT_BTREE_NODE *result_node, int *result_idx);

int traverse_ddt_btree(DDT_BTREE_NODE *tnode, int fd);

int insert_ddt_btree(unsigned char *key, DDT_BTREE_NODE *tnode, int fd,
				DDT_BTREE_META *this_meta);

int _insert_non_full_ddt_btree(DDT_BTREE_EL *new_element,
				DDT_BTREE_NODE *tnode, int fd, DDT_BTREE_META *this_meta);

int _split_child_ddt_btree(DDT_BTREE_NODE *pnode, int s_idx,
				DDT_BTREE_NODE *cnode, int fd, DDT_BTREE_META *this_meta);

int delete_ddt_btree(unsigned char *key, DDT_BTREE_NODE *tnode,
				int fd, DDT_BTREE_META *this_meta);

int _extract_largest_child(DDT_BTREE_NODE *tnode, int fd, DDT_BTREE_NODE *result_node,
				DDT_BTREE_EL *result_el, DDT_BTREE_META *this_meta);

int _rebalance_btree(DDT_BTREE_NODE *tnode, int selected_child, int fd,
				DDT_BTREE_META *this_meta);

int increase_el_refcount(DDT_BTREE_NODE *tnode, int s_idx, int fd);

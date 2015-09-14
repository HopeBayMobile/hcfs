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

#ifndef GW20_HCFS_DEDUP_TABLE_H_
#define GW20_HCFS_DEDUP_TABLE_H_


#include <openssl/sha.h>
#include <stdio.h>

/*
 * The structure of ddt is a combination of hash table and btree. Each node
 * of hash table contains a pointer to the root of a independent btree.
 *
 * To search/insert element from/to ddt, we must choose a node of hash table,
 * by hash function, and then put it to the btree.
 *
 *
 * Obj_id contains sha256 hash and parts of the content of object. To handle
 * cases of collision, the first and last bytes of the content of object are
 * appended to the obj_id for verfication.
 *
 * Obj_id -
 *     -------------------------------------------------------
 *     |                            |            |           |
 *     |  32 Bytes for sha256 hash  | First byte | Last byte |
 *     |                            | of object  | of object |
 *     -------------------------------------------------------
 *
 */
#define OBJID_LENGTH (SHA256_DIGEST_LENGTH + (BYTES_TO_CHECK * 2))
/* Hash key str size */
#define OBJID_STRING_LENGTH (OBJID_LENGTH * 2 + 1)
/* Addtionial bytes to check - To avoid collision cases */
#define BYTES_TO_CHECK 1

/* Define the upper/lower bound of btree elements */
#define MAX_EL_PER_NODE 70
#define MIN_EL_PER_NODE 30


typedef struct {
	unsigned char obj_id[OBJID_LENGTH];
	off_t obj_size;
	//char start_bytes[BYTES_TO_CHECK];
	//char end_bytes[BYTES_TO_CHECK];
	long long refcount;
} DDT_BTREE_EL;

typedef struct {
	int num_el;
	int is_leaf;
	long long this_node_pos;
	// Elements store in this node
	DDT_BTREE_EL ddt_btree_el[MAX_EL_PER_NODE];
	// Parent node
	long long parent_node_pos;
	// Child nodes
	long long child_node_pos[MAX_EL_PER_NODE + 1];
	// Point to next reclaimed node
	long long gc_list_next;
} DDT_BTREE_NODE;

typedef struct {
	long long tree_root;
	long long total_el;
	// Point to the first node which can be reclaimed
	long long node_gc_list;
} DDT_BTREE_META;


int initialize_ddt_meta(char *meta_path);

FILE* get_ddt_btree_meta(unsigned char key[], DDT_BTREE_NODE *root,
				DDT_BTREE_META *this_meta);

int search_ddt_btree(unsigned char key[], DDT_BTREE_NODE *tnode, int fd,
				DDT_BTREE_NODE *result_node, int *result_idx);

int traverse_ddt_btree(DDT_BTREE_NODE *tnode, int fd);

int insert_ddt_btree(unsigned char key[], const off_t obj_size,
				DDT_BTREE_NODE *tnode, int fd,
				DDT_BTREE_META *this_meta);

int delete_ddt_btree(unsigned char key[], DDT_BTREE_NODE *tnode,
				int fd, DDT_BTREE_META *this_meta, int force_delete);

int increase_ddt_el_refcount(DDT_BTREE_NODE *tnode, int s_idx, int fd);

int decrease_ddt_el_refcount(unsigned char key[], DDT_BTREE_NODE *tnode,
				int fd, DDT_BTREE_META *this_meta);

// Util function for data dedup
int get_obj_id(char *path, unsigned char *hash, unsigned char start_bytes[],
				unsigned char end_bytes[], off_t *obj_size);

int obj_id_to_string(unsigned char obj_id[OBJID_LENGTH],
				char output_str[OBJID_STRING_LENGTH]);

#endif /* GW20_HCFS_DEDUP_TABLE_H_ */

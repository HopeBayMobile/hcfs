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
#include <inttypes.h>

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
 *     -------------------------------------------------------------
 *     |                            |               |              |
 *     |  32 Bytes for sha256 hash  | First n bytes | Last n bytes |
 *     |                            | of object     | of object    |
 *     -------------------------------------------------------------
 *
 */
#define OBJID_LENGTH (SHA256_DIGEST_LENGTH + (BYTES_TO_CHECK * 2))
/* Hash key str size */
#define OBJID_STRING_LENGTH (OBJID_LENGTH * 2 + 1)
/* Addtionial bytes to check - To avoid collision cases */
#define BYTES_TO_CHECK 4

/* Define the upper/lower bound of btree elements */
#define MAX_EL_PER_NODE 70
#define MIN_EL_PER_NODE 30


typedef struct {
	uint8_t obj_id[OBJID_LENGTH];
	off_t obj_size;
	//char start_bytes[BYTES_TO_CHECK];
	//char end_bytes[BYTES_TO_CHECK];
	int64_t refcount;
} DDT_BTREE_EL;

typedef struct {
	int32_t num_el;
	int32_t is_leaf;
	int64_t this_node_pos;
	// Elements store in this node
	DDT_BTREE_EL ddt_btree_el[MAX_EL_PER_NODE];
	// Parent node
	int64_t parent_node_pos;
	// Child nodes
	int64_t child_node_pos[MAX_EL_PER_NODE + 1];
	// Point to next reclaimed node
	int64_t gc_list_next;
} DDT_BTREE_NODE;

typedef struct {
	int64_t tree_root;
	int64_t total_el;
	// Point to the first node which can be reclaimed
	int64_t node_gc_list;
} DDT_BTREE_META;


int32_t initialize_ddt_meta(char *meta_path);

FILE* get_ddt_btree_meta(uint8_t key[], DDT_BTREE_NODE *root,
				DDT_BTREE_META *this_meta);

int32_t search_ddt_btree(uint8_t key[], DDT_BTREE_NODE *tnode, int32_t fd,
				DDT_BTREE_NODE *result_node, int32_t *result_idx);

int32_t traverse_ddt_btree(DDT_BTREE_NODE *tnode, int32_t fd);

int32_t insert_ddt_btree(uint8_t key[], const off_t obj_size,
				DDT_BTREE_NODE *tnode, int32_t fd,
				DDT_BTREE_META *this_meta);

int32_t delete_ddt_btree(uint8_t key[], DDT_BTREE_NODE *tnode,
				int32_t fd, DDT_BTREE_META *this_meta, int32_t force_delete);

int32_t increase_ddt_el_refcount(DDT_BTREE_NODE *tnode, int32_t s_idx, int32_t fd);

int32_t decrease_ddt_el_refcount(uint8_t key[], DDT_BTREE_NODE *tnode,
				int32_t fd, DDT_BTREE_META *this_meta);

// Util function for data dedup
int32_t get_obj_id(char *path, uint8_t *hash, uint8_t start_bytes[],
				uint8_t end_bytes[], off_t *obj_size);

int32_t obj_id_to_string(uint8_t obj_id[OBJID_LENGTH],
				char output_str[OBJID_STRING_LENGTH]);

#endif /* GW20_HCFS_DEDUP_TABLE_H_ */

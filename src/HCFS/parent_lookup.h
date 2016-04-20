/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: parent_lookup.h
* Abstract: The header file for looking up parents
*
* Revision History
* 2015/11/19 Jiahong created this file
* 2015/11/19 Jiahong moved parent lookup content from path_reconstruct
*
**************************************************************************/
#ifndef GW20_HCFS_PARENT_LOOKUP_H_
#define GW20_HCFS_PARENT_LOOKUP_H_

#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <stdint.h>

#include "params.h"

#define MAX_PARENTS_PER_PAGE 32
#define PLOOKUP_HASH_NUM_ENTRIES 1024
typedef struct {
	ino_t parentinode;
	uint8_t haveothers;
} PRIMARY_PARENT_T;

typedef struct {
	ino_t thisinode;
	int8_t num_parents;
	ino_t parents[MAX_PARENTS_PER_PAGE];
	int64_t nextlookup;  /* File pos of the next inode in the same hash */
	int64_t nextpage; /* File pos of the next page for this inode */
	int64_t gc_next;
} PLOOKUP_PAGE_T;

typedef struct {
	int64_t hash_head[PLOOKUP_HASH_NUM_ENTRIES];
	int64_t gc_head;
} PLOOKUP_HEAD_T;

PLOOKUP_HEAD_T parent_lookup_head;	

FILE *plookup2_fptr;
/* API for calling from outside */

int fetch_all_parents(ino_t self_inode, int *parentnum, ino_t **parentlist);
int lookup_add_parent(ino_t self_inode, ino_t parent_inode);
int lookup_delete_parent(ino_t self_inode, ino_t parent_inode);
int lookup_replace_parent(ino_t self_inode, ino_t parent_inode1,
			  ino_t parent_inode2);

#endif  /* GW20_HCFS_PARENT_LOOKUP_H_ */

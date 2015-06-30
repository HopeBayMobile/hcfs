/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dir_meta_defs.h
* Abstract: The header file for definition of directory meta file
*
* Revision History
* 2015/6/30 Jiahong created the file and moved content to this file
* from fuseop.h.
*
**************************************************************************/

#ifndef GW20_HCFS_DIR_META_DEFS_H_
#define GW20_HCFS_DIR_META_DEFS_H_


#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "params.h"
#include "global.h"

/*BEGIN META definition*/

/* Defining parameters for B-tree operations (dir entries). */
/* Max number of children per node is 100, min is 50, so at least 49 elements
*  in each node (except the root) */
#define MAX_DIR_ENTRIES_PER_PAGE 99
/* Minimum number of entries before an underflow */
#define MIN_DIR_ENTRIES_PER_PAGE 30
/* WARNING: MIN_DIR_ENTRIES_PER_PAGE must be smaller than
*  MAX_DIR_ENTRIES_PER_PAGE/2 */

/* Defining the names for file object types */
#define D_ISDIR 0
#define D_ISREG 1
#define D_ISLNK 2

/* Structures for directories */
/* Defining directory entry in meta files*/
typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN+1];
	char d_type;
} DIR_ENTRY;

/* Defining the structure of directory object meta */
typedef struct {
	long long total_children;  /*Total children not including "." and "..*/
	long long root_entry_page;
	long long next_xattr_page;
	long long entry_page_gc_list;
	long long tree_walk_list_head;
	unsigned long generation;
	unsigned long metaver;
} DIR_META_TYPE;

/* Defining the structure for a page of directory entries */
typedef struct {
	int num_entries;
	DIR_ENTRY dir_entries[MAX_DIR_ENTRIES_PER_PAGE];
	long long this_page_pos; /*File pos of the current node*/
	/* File pos of child pages for this node, b-tree style */
	long long child_page_pos[MAX_DIR_ENTRIES_PER_PAGE+1];
	/*File pos of parent. If this is the root, the value is 0 */
	long long parent_page_pos;
	/*File pos of the next gc entry if on gc list*/
	long long gc_list_next;
	long long tree_walk_next;
	long long tree_walk_prev;
} DIR_ENTRY_PAGE;

/*END META definition*/


#endif  /* GW20_HCFS_DIR_META_DEFS_H_ */

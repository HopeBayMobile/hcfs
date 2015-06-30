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

#ifndef GW20_HCFS_FILE_META_DEFS_H_
#define GW20_HCFS_FILE_META_DEFS_H_

#include "global.h"

/*BEGIN META definition*/

/* Number of pointers in a pointer page */
#define POINTERS_PER_PAGE 1024

/* Defining the names for block status */
/* Not stored on any media or storage. Value should be zero.*/
#define ST_NONE 0
/* Stored only on local cache */
#define ST_LDISK 1
/* Stored only on cloud storage */
#define ST_CLOUD 2
/* Stored both on local cache and cloud storage */
#define ST_BOTH 3
/* In transition from local cache to cloud storage */
#define ST_LtoC 4
/* In transition from cloud storage to local cache */
#define ST_CtoL 5
/* Block to be deleted in backend */
#define ST_TODELETE 6

#define MAX_BLOCK_ENTRIES_PER_PAGE 100

/* Structures for regular files */
/* Defining one block status entry in meta files */
typedef struct {
	unsigned char status;
} BLOCK_ENTRY;

/* Defining the structure of one page of block status page */
typedef struct {
	int num_entries;
	BLOCK_ENTRY block_entries[MAX_BLOCK_ENTRIES_PER_PAGE];
} BLOCK_ENTRY_PAGE;

/* Defining the structure of pointer page (pointers to other pages) */
typedef struct {
	long long ptr[POINTERS_PER_PAGE];
} PTR_ENTRY_PAGE;

/* Defining the structure of file meta */
typedef struct {
	long long next_xattr_page;
	long long direct;
	long long single_indirect;
	long long double_indirect;
	long long triple_indirect;
	long long quadruple_indirect;
	unsigned long generation;
	unsigned long metaver;
} FILE_META_TYPE;

/*END META definition*/

#endif  /* GW20_HCFS_FILE_META_DEFS_H_ */

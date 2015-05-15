/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: lookup_count.h
* Abstract: The c header file for the lookup count table (for inodes).
*
* Revision History
* 2015/5/15 Jiahong created the file.
*
**************************************************************************/

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_LOOKUP_ENTRIES 65536

typedef struct lookup_template {
	ino_t this_inode;
	int lookup_count;
	char to_delete;
	char d_type;
	struct lookup_template *next;
} LOOKUP_NODE_TYPE;

typedef struct {
	sem_t entry_sem;
	LOOKUP_NODE_TYPE *head;
} LOOKUP_HEAD_TYPE;

LOOKUP_HEAD_TYPE lookup_table[NUM_LOOKUP_ENTRIES];

int lookup_init();
int lookup_increase(ino_t this_inode, int amount, char d_type);
int lookup_decrease(ino_t this_inode, int amount, char *d_type,
				char *need_delete);
int lookup_markdelete(ino_t this_inode);

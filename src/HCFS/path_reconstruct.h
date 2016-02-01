/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: path_reconstruct.h
* Abstract: The header file for reconstructing path from parent lookups
*
* Revision History
* 2015/10/26 Jiahong created this file
* 2015/11/19 Jiahong moved parent lookup content to parent_lookup
*
**************************************************************************/
#ifndef GW20_HCFS_PATH_RECONSTRUCT_H_
#define GW20_HCFS_PATH_RECONSTRUCT_H_

#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>

#include "params.h"

#define MAX_LOOKUP_NODES 4096
#define NUM_LOOKUP_ENTRY 4096
#define NUM_NODES_DROP 128

struct path_lookup {
	ino_t child;
	ino_t parent;
	char childname[MAX_FILENAME_LEN+1];
	long long lookupcount;
	struct path_lookup *prev;
	struct path_lookup *next;
	struct path_lookup *gprev;
	struct path_lookup *gnext;
	struct path_lookup *self;
};
typedef struct path_lookup PATH_LOOKUP;

typedef struct {
	PATH_LOOKUP *first;
	PATH_LOOKUP *last;
} PATH_HEAD_ENTRY;

/* There could be multiple such hash table, one for each mounted volume */
typedef struct {
	PATH_HEAD_ENTRY hashtable[NUM_LOOKUP_ENTRY];
	PATH_LOOKUP *gfirst;
	int num_nodes;
	sem_t pathcache_lock;
	ino_t root_inode;
} PATH_CACHE;

sem_t pathlookup_data_lock;
FILE *pathlookup_data_fptr;

/* API for calling from outside */
PATH_CACHE * init_pathcache(ino_t root_inode);
int destroy_pathcache(PATH_CACHE *cacheptr);

int lookup_name(PATH_CACHE *cacheptr, ino_t thisinode, PATH_LOOKUP *retnode);
int construct_path_iterate(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
		int bufsize);

int construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
                   ino_t rootinode);

int delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete);

int init_pathlookup(void);
void destroy_pathlookup(void);
int pathlookup_write_parent(ino_t self_inode, ino_t parent_inode);
int pathlookup_read_parent(ino_t self_inode, ino_t *parentptr);

#endif  /* GW20_HCFS_PATH_RECONSTRUCT_H_ */


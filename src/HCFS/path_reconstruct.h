/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: path_reconstruct.h
* Abstract: The header file for reconstructing path from parent lookups
*
* Revision History
* 2015/10/26 Jiahong created this file
*
**************************************************************************/
#ifndef GW20_HCFS_PATH_RECONSTRUCT_H_
#define GW20_HCFS_PATH_RECONSTRUCT_H_

#define MAX_LOOKUP_NODES 4096
#define NUM_LOOKUP_ENTRY 4096
#define NUM_NODES_DROP 128

struct path_lookup {
	ino_t child;
	ino_t parent;
	char childname[256];
	long long lookupcount;
	struct path_lookup *prev;
	struct path_lookup *next;
	struct path_lookup *gprev;
	struct path_lookup *gnext;
};
typedef struct path_lookup PATH_LOOKUP;

typedef struct {
	PATH_LOOKUP *first;
	PATH_LOOKUP *last;
} PATH_HEAD_ENTRY;

/* There could be multiple such hash table, one for each mounted volume */
typedef struc {
	PATH_HEAD_ENTRY hashtable[NUM_LOOKUP_ENTRY];
	PATH_LOOKUP *gfirst;
	int num_nodes;
	sem_t pathcache_lock;
	ino_t root_inode;
} PATH_CACHE;

/* API for calling from outside */
PATH_CACHE * init_pathcache(ino_t root_inode);
int destroy_pathcache(PATH_CACHE *cacheptr);

int delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete);
int lookup_name(PATH_CACHE *cacheptr, ino_t thisinode, PATH_LOOKUP *retnode);
int construct_path_iterate(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
		int bufsize);

int construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result);

#endif  /* GW20_HCFS_PATH_RECONSTRUCT_H_ */


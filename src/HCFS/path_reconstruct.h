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

struct path_lookup {
	ino_t child;
	ino_t parent;
	char childname[256];
	long long lookupcount;
	struct path_lookup *prev;
	struct path_lookup *next;
};
typedef struct path_lookup PATH_LOOKUP;

typedef struct {
	PATH_LOOKUP *first;
	PATH_LOOKUP *last;
	int num_nodes;
} PATH_HEAD_ENTRY;

PATH_HEAD_ENTRY pathcache[NUM_LOOKUP_ENTRY];
sem_t pathcache_lock;

/* API for calling from outside */
int init_pathcache();
int destroy_pathcache();

int construct_path(ino_t thisinode, char **result, int bufsize);
int lookup_name(ino_t thisinode, char *namebuf);


#endif  /* GW20_HCFS_PATH_RECONSTRUCT_H_ */


/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: path_reconstruct.c
* Abstract: The c source file for reconstructing path from parent lookups
*
* Revision History
* 2015/10/26 Jiahong created this file
*
**************************************************************************/

#include "path_reconstruct.h"

#include <errno.h>
#include <string.h>
#include <semaphore.h>

#include "logger.h"
/************************************************************************
*
* Function name: init_pathcache
*        Inputs: ino_t root_inode
*       Summary: Allocate a new cache for inode to path lookup
*  Return value: Pointer to the new path cache structure if successful.
*                NULL otherwise.
*
*************************************************************************/
PATH_CACHE * init_pathcache(ino_t root_inode);
{
	int ret, errcode;
	PATH_CACHE *tmpptr;

	tmpptr = malloc(sizeof(PATH_CACHE));
	if (tmpptr == NULL) {
		write_log(0, "Out of memory\n");
		goto errcode_handle;
	}

	ret = sem_init(&(tmpptr->pathcache_lock), 0, 1);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Error: %d, %s\n", errcode, strerror(errcode));
		goto errcode_handle;
	}

	memset((tmpptr->pathcache), 0,
		sizeof(PATH_HEAD_ENTRY) * NUM_LOOKUP_ENTRY);
	tmpptr->gfirst = NULL;
	tmpptr->num_nodes = 0;
	tmpptr->root_inode = root_inode;

	return tmpptr;

errcode_handle:
	if (tmpptr != NULL)
		free(tmpptr);
	return NULL;
}
/************************************************************************
*
* Function name: destroy_pathcache
*        Inputs: PATH_CACHE *cacheptr
*       Summary: Free up cache pointed by "cacheptr" for inode to path lookup
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int destroy_pathcache(PATH_CACHE *cacheptr)
{
	PATH_LOOKUP *tmp, *next;
	int count, ret, errcode;

	if (cacheptr == NULL)
		return -EINVAL;

	ret = sem_wait(&(cacheptr->pathcache_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error. Code %d, %s\n",
			errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	for (count = 0; count < NUM_LOOKUP_ENTRY; count++) {
		tmp = (cacheptr->pathcache[count]).first;

		while (tmp != NULL) {
			next = tmp->next;
			free(tmp);
			tmp = next;
		}
	}
	sem_post(&(cacheptr->pathcache_lock));
	free(cacheptr);
	return 0;

errcode_handle:
	return errcode;
}

/* Internal function for dropping a certain number of least-referenced
entries */
void drop_cache_entry(PATH_CACHE *cacheptr)
{
	int num_dropped, hashval;
	PATH_LOOKUP *tmpptr;
	int ret, errcode;

	num_dropped = 0;
	tmpptr = cacheptr->gfirst;
	while (num_dropped < NUM_NODES_DROP) {
		if (cacheptr->num_nodes <= 0)
			break;
		cacheptr->gfirst = tmpptr->gnext;
		if (cacheptr->gfirst != NULL)
			(cacheptr->gfirst)->gprev = NULL;
		if (tmpptr->prev == NULL) {
			hashval = (tmpptr->child) % NUM_LOOKUP_ENTRY;
			(cacheptr->hashtable[hashval]).first = tmpptr->next;
		} else {
			(tmpptr->prev)->next = tmpptr->next;
		}
		if (tmpptr->next == NULL) {
			hashval = (tmpptr->child) % NUM_LOOKUP_ENTRY;
			(cacheptr->hashtable[hashval]).last = tmpptr->prev;
		} else {
			(tmpptr->next)->prev = tmpptr->prev;
		}
		free(tmpptr);
		(cacheptr->num_nodes)--;
		num_dropped++;
	}
	return;
}

/* Internal function for adding a cache entry. */
/* Queue the new node to the tail of the ring */
int add_cache_entry(PATH_CACHE *cacheptr, PATH_LOOKUP *newnode)
{
	int ret, errcode;
	int hashindex;

	ret = sem_wait(&(cacheptr->pathcache_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error. Code %d, %s\n",
			errcode, strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	if (cacheptr->num_nodes >= MAX_LOOKUP_NODES)
		drop_cache_entry(cacheptr);

	hashindex = (newnode->child) % NUM_LOOKUP_ENTRY;

	/* Change the ring structure */
	((cacheptr->hashtable[hashindex]).last)->next = newnode;
	newnode->prev = cacheptr->hashtable[hashindex]).last;
	newnode->next = NULL;
	cacheptr->hashtable[hashindex]).last = newnode;

	/* Change the global linked list */
	(cacheptr->gfirst)->gprev = newnode;
	newnode->gnext = cacheptr->gfirst;
	newnode->gprev = NULL;
	cacheptr->gfirst = newnode;
	/* TODO: Decide where to actually lock the pathcache lock and where
		to release */
	(cacheptr->num_nodes)++;
	sem_post(&(cacheptr->pathcache_lock));
	return 0;
errcode_handle:
	return errcode;
}

int delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete);


int lookup_name(PATH_CACHE *cacheptr, ino_t thisinode, PATH_LOOKUP *retnode);


int construct_path_iterate(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
		int bufsize)
{
	char *current_path;
	char thisname[256];
	PATH_LOOKUP cachenode;
	ino_t parent_inode;
	int ret, pathlen;

	ret = lookup_name(cacheptr, thisinode, &cachenode);

	if (ret < 0) {
		...
	}

	parent_inode = cachenode.parent;
	current_path = calloc(bufsize + strlen(cachenode.childname) + 1,
				sizeof(char));
	if (current_path == NULL) {
		...
	}
	pathlen = bufsize + strlen(cachenode.childname) + 1;

/* TODO: Check if the path name exceed the max allowed */
	snprinf(current_path, pathlen, "%s/%s", cachenode.childname, *result);
/* TODO: Check if parent inode is root. If so, add the root symbol
to the path and return */
	if (parent_inode == cacheptr->root_inode) {
		...
	}

}

int construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result);


#endif  /* GW20_HCFS_PATH_RECONSTRUCT_H_ */


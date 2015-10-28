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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "logger.h"
#include "meta_mem_cache.h"
#include "fuseop.h"
#include "macro.h"

extern SYSTEM_CONF_STRUCT system_config;

/************************************************************************
*
* Function name: init_pathcache
*        Inputs: ino_t root_inode
*       Summary: Allocate a new cache for inode to path lookup
*  Return value: Pointer to the new path cache structure if successful.
*                NULL otherwise.
*
*************************************************************************/
PATH_CACHE * init_pathcache(ino_t root_inode)
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

	memset((tmpptr->hashtable), 0,
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
		tmp = (cacheptr->hashtable[count]).first;

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
}

/* Internal function for adding a cache entry. */
/* Queue the new node to the tail of the ring */
void add_cache_entry(PATH_CACHE *cacheptr, PATH_LOOKUP *newnode)
{
	int hashindex;

	if (cacheptr->num_nodes >= MAX_LOOKUP_NODES)
		drop_cache_entry(cacheptr);

	hashindex = (newnode->child) % NUM_LOOKUP_ENTRY;

	/* Change the ring structure */
	((cacheptr->hashtable[hashindex]).last)->next = newnode;
	newnode->prev = (cacheptr->hashtable[hashindex]).last;
	newnode->next = NULL;
	(cacheptr->hashtable[hashindex]).last = newnode;

	/* Change the global linked list */
	(cacheptr->gfirst)->gprev = newnode;
	newnode->gnext = cacheptr->gfirst;
	newnode->gprev = NULL;
	cacheptr->gfirst = newnode;
	(cacheptr->num_nodes)++;
}

/* Helper function for repositioning the node according to usage */
void _node_reposition(PATH_LOOKUP *tmpptr)
{
	PATH_LOOKUP *tmpprev, *tmpgnext;
	while (tmpptr->prev != 0) {
		if ((tmpptr->lookupcount) > ((tmpptr->prev)->lookupcount)) {
			/* Exchange tmpptr with tmpptr->prev */
			tmpprev = tmpptr->prev;
			tmpptr->prev = tmpprev->prev;
			tmpprev->next = tmpptr->next;
			tmpptr->next = tmpprev;
			tmpprev->prev = tmpptr;
		} else {
			break;
		}
	}

	while (tmpptr->gnext != 0) {
		if ((tmpptr->lookupcount) > ((tmpptr->gnext)->lookupcount)) {
			/* Exchange tmpptr with tmpptr->gnext */
			tmpgnext = tmpptr->gnext;
			tmpptr->gnext = tmpgnext->gnext;
			tmpgnext->gprev = tmpptr->gprev;
			tmpptr->gprev = tmpgnext;
			tmpgnext->gnext = tmpptr;
		} else {
			break;
		}
	}
}

int search_inode(ino_t parent, ino_t child, DIR_ENTRY *dentry)
{
	META_CACHE_ENTRY_STRUCT *cache_entry;
	DIR_META_TYPE tempmeta;
	DIR_ENTRY_PAGE temp_page;
	int ret, errcode;
	int count;

	cache_entry = meta_cache_lock_entry(parent);
	if (cache_entry == NULL)
		return -ENOMEM;

	ret = meta_cache_lookup_dir_data(parent, NULL, &tempmeta,
						NULL, cache_entry);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	temp_page.this_page_pos = tempmeta.tree_walk_list_head;
	while (temp_page.this_page_pos != 0) {
		ret = meta_cache_lookup_dir_data(parent, NULL, NULL,
						&temp_page, cache_entry);
		if (ret < 0) {
			errcode = ret;
			goto errcode_handle;
		}

		for (count = 0; count < temp_page.num_entries; count++)
			if (temp_page.dir_entries[count].d_ino == child)
				break;
		if (count < temp_page.num_entries) {
			memcpy(dentry, &(temp_page.dir_entries[count]),
				sizeof(DIR_ENTRY));
			break;
		}
		temp_page.this_page_pos = temp_page.tree_walk_next;
	}
	if (temp_page.this_page_pos == 0) {
		errcode = -ENOENT;
		goto errcode_handle;
	}

	meta_cache_close_file(cache_entry);
	meta_cache_unlock_entry(cache_entry);

	return 0;
errcode_handle:
	meta_cache_close_file(cache_entry);
	meta_cache_unlock_entry(cache_entry);
	return errcode;
}
			
/************************************************************************
*
* Function name: lookup_name
*        Inputs: PATH_CACHE *cacheptr, ino_t thisinode, PATH_LOOKUP *retnode
*       Summary: Lookup the path cache entry for thisinode. If cannot be
*                found, use pathlookup_read_parent to find the parent inode,
*                then read dir file to find the name of thisinode. The result
*                is constructed into a new node and added to the cache.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int lookup_name(PATH_CACHE *cacheptr, ino_t thisinode, PATH_LOOKUP *retnode)
{
	int ret, errcode;
	int hashindex;
	PATH_LOOKUP *tmpptr;
	ino_t parentinode;
	DIR_ENTRY tmpentry;

	hashindex = thisinode % NUM_LOOKUP_ENTRY;

	tmpptr = (cacheptr->hashtable[hashindex]).first;

	while (tmpptr != NULL) {
		if (thisinode == tmpptr->child) {
			/* We found the node. Update the lookup count
			and the position if needed, and return the result */
			memcpy(retnode, tmpptr, sizeof(PATH_LOOKUP));
			(tmpptr->lookupcount)++;
			_node_reposition(tmpptr);
			return 0;
		}
		tmpptr = tmpptr->next;
	}
	/* Did not find the node. Proceed to lookup and then add to cache */

	ret = pathlookup_read_parent(thisinode, &parentinode);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	ret = search_inode(parentinode, thisinode, &tmpentry);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	tmpptr = malloc(sizeof(PATH_LOOKUP));
	if (tmpptr == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory\n");
		goto errcode_handle;
	}

	tmpptr->child = thisinode;
	tmpptr->parent = parentinode;
	snprintf((tmpptr->childname), MAX_FILENAME_LEN+1, "%s",
		tmpentry.d_name);
	tmpptr->lookupcount = 1;

	/* Add new node to cache */
	add_cache_entry(cacheptr, tmpptr);
	memcpy(retnode, tmpptr, sizeof(PATH_LOOKUP));
	return 0;
errcode_handle:
	return errcode;
}


/************************************************************************
*
* Function name: construct_path_iterate
*        Inputs: PATH_CACHE *cacheptr, ino_t thisinode, char **result,
*                int bufsize
*       Summary: Recursively prepend the name of thisinode to the
*                pathname pointed by *result. bufsize indicates the
*                buffer size storing the pathname.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int construct_path_iterate(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
		int bufsize)
{
	char *current_path, *tmpptr;
	PATH_LOOKUP cachenode;
	ino_t parent_inode;
	int ret, pathlen, errcode;

	ret = lookup_name(cacheptr, thisinode, &cachenode);

	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	parent_inode = cachenode.parent;
	current_path = calloc(bufsize + strlen(cachenode.childname) + 1,
				sizeof(char));
	if (current_path == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of memory\n");
		goto errcode_handle;
	}
	pathlen = bufsize + strlen(cachenode.childname) + 1;

/* TODO: Check if the path name exceed the max allowed */
	snprintf(current_path, pathlen, "%s/%s", cachenode.childname, *result);

	if (parent_inode == cacheptr->root_inode) {
		tmpptr = *result;
		*result = current_path;
		if (tmpptr != NULL)
			free(tmpptr);
		current_path = calloc(pathlen + 1, sizeof(char));
		if (current_path == NULL) {
			errcode = -ENOMEM;
			write_log(0, "Out of memory\n");
			goto errcode_handle;
		}
		pathlen = pathlen + 1;
		snprintf(current_path, pathlen, "/%s", *result);
		tmpptr = *result;
		*result = current_path;
		free(tmpptr);
		return 0;
	}
	tmpptr = *result;
	*result = current_path;
	if (tmpptr != NULL)
		free(tmpptr);
	return construct_path_iterate(cacheptr, parent_inode, result, pathlen);
errcode_handle:
	tmpptr = *result;
	if (tmpptr != NULL)
		free(tmpptr);
	return errcode;
}

/************************************************************************
*
* Function name: construct_path
*        Inputs: PATH_CACHE *cacheptr, ino_t thisinode, char **result
*       Summary: Construct the path from root to thisinode, and return
*                the path via the pointer in *result.
*          Note: The caller is responsible for freeing the memory storing
*                the result string.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result)
{
	int ret, errcode;
	char *tmpbuf;

	ret = sem_wait(&(cacheptr->pathcache_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error. Code %d, %s\n",
			errcode, strerror(errcode));
		errcode = -errcode;
		return errcode;
	}
	tmpbuf = calloc(10, sizeof(char));
	if (tmpbuf == NULL) {
		errcode = -ENOMEM;
		write_log(0, "Out of Memory\n");
		goto errcode_handle;
	} 

	*result = tmpbuf;
	ret = construct_path_iterate(cacheptr, thisinode, result, 10);
	if (ret < 0) {
		errcode = ret;
		goto errcode_handle;
	}

	sem_post(&(cacheptr->pathcache_lock));
	return 0;

errcode_handle:
	sem_post(&(cacheptr->pathcache_lock));
	return errcode;
}

/************************************************************************
*
* Function name: delete_pathcache_node
*        Inputs: PATH_CACHE *cacheptr, ino_t todelete
*       Summary: Delete inode "todelete" from cache if found in cache.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
/* TODO: Need to delete cache node in forget routine (when no meta cache
is locked) */
/* TODO: Should reset the parent in pathlookup as well */
int delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete)
{
	int ret, errcode;
	int hashindex;
	PATH_LOOKUP *tmpptr;

	ret = sem_wait(&(cacheptr->pathcache_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error. Code %d, %s\n",
			errcode, strerror(errcode));
		errcode = -errcode;
		return errcode;
	}

	hashindex = todelete % NUM_LOOKUP_ENTRY;

	tmpptr = (cacheptr->hashtable[hashindex]).first;

	while (tmpptr != NULL) {
		if (todelete == tmpptr->child) {
			/* We found the node. Delete it */
			if (tmpptr->next != NULL)
				(tmpptr->next)->prev = tmpptr->prev;
			else
				(cacheptr->hashtable[hashindex]).last =
						tmpptr->prev;
			if (tmpptr->prev != NULL)
				(tmpptr->prev)->next = tmpptr->next;
			else
				(cacheptr->hashtable[hashindex]).first =
						tmpptr->next;
			if (tmpptr->gnext != NULL)
				(tmpptr->gnext)->gprev = tmpptr->gprev;
			if (tmpptr->gprev != NULL)
				(tmpptr->gprev)->gnext = tmpptr->gnext;
			else
				cacheptr->gfirst = tmpptr->gnext;

			free(tmpptr);
			sem_post(&(cacheptr->pathcache_lock));
			return 0;
		}
		tmpptr = tmpptr->next;
	}
	/* Did not find the node. Just return without an error.*/

	sem_post(&(cacheptr->pathcache_lock));
	return 0;
}

/************************************************************************
*
* Function name: init_pathlookup
*        Inputs: None
*       Summary: Init path lookup component for HCFS
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int init_pathlookup()
{
	int ret, errcode;
	char pathname[METAPATHLEN+10];

	ret = sem_init(&(pathlookup_data_lock), 0, 1);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error in init: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}

	snprintf(pathname, METAPATHLEN, "%s/pathlookup_db", METAPATH);
	if (access(pathname, F_OK) != 0) {
		MKNOD(pathname, S_IFREG | 0600, 0)
	}
	pathlookup_data_fptr = fopen(pathname, "r+");
	if (pathlookup_data_fptr == NULL) {
		errcode = errno;
		write_log(0, "Unexpected error in init: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	setbuf(pathlookup_data_fptr, NULL);
	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: destroy_pathlookup
*        Inputs: None
*       Summary: Cleanup path lookup component for HCFS when shutting down
*  Return value: None
*
*************************************************************************/
void destroy_pathlookup()
{
	fclose(pathlookup_data_fptr);
	sem_destroy(&(pathlookup_data_lock));
	return;
}

/************************************************************************
*
* Function name: pathlookup_write_parent
*        Inputs: ino_t self_inode, ino_t parent_inode
*       Summary: Writes the parent inode to a file in metastorage
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int pathlookup_write_parent(ino_t self_inode, ino_t parent_inode)
{
	off_t filepos;
	ino_t tmpinode;
	int errcode, ret;
	ssize_t ret_ssize;

	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		return errcode;
	}

	filepos = (off_t) ((self_inode - 1) * sizeof(ino_t));
	tmpinode = parent_inode;
	PWRITE(fileno(pathlookup_data_fptr), &tmpinode, sizeof(ino_t), filepos);

	sem_post(&(pathlookup_data_lock));
	return 0;
errcode_handle:
	sem_post(&(pathlookup_data_lock));
	return errcode;
}

/************************************************************************
*
* Function name: pathlookup_read_parent
*        Inputs: ino_t self_inode, ino_t *parentptr
*       Summary: Reads the parent inode from a file in metastorage and store
*                in the ino_t structure pointed by parentptr
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int pathlookup_read_parent(ino_t self_inode, ino_t *parentptr)
{
	off_t filepos;
	ino_t tmpinode;
	int errcode, ret;
	ssize_t ret_ssize;

	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		return errcode;
	}

	filepos = (off_t) ((self_inode - 1) * sizeof(ino_t));
	PREAD(fileno(pathlookup_data_fptr), &tmpinode, sizeof(ino_t), filepos);

	sem_post(&(pathlookup_data_lock));
	*parentptr = tmpinode;
	return 0;
errcode_handle:
	sem_post(&(pathlookup_data_lock));
	return errcode;
}


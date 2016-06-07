/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: path_reconstruct.c
* Abstract: The c source file for reconstructing path from parent lookups
*
* Revision History
* 2015/10/26 Jiahong created this file
* 2016/1/18 Jiahong modified the file to change lookup routines and log
*
**************************************************************************/

#include "path_reconstruct.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>

#include "logger.h"
#include "meta_mem_cache.h"
#include "fuseop.h"
#include "macro.h"
#include "parent_lookup.h"
#include "global.h"
#include "utils.h"

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
	int32_t ret, errcode;
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
int32_t destroy_pathcache(PATH_CACHE *cacheptr)
{
	PATH_LOOKUP *tmp, *next;
	int32_t count, ret, errcode;

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
	int32_t num_dropped, hashval;
	PATH_LOOKUP *tmpptr;

	num_dropped = 0;
	while (num_dropped < NUM_NODES_DROP) {
		tmpptr = cacheptr->gfirst;
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
	int32_t hashindex;

	if (cacheptr->num_nodes >= MAX_LOOKUP_NODES)
		drop_cache_entry(cacheptr);

	hashindex = (newnode->child) % NUM_LOOKUP_ENTRY;

	/* Change the ring structure */
	if ((cacheptr->hashtable[hashindex]).last != NULL)
		((cacheptr->hashtable[hashindex]).last)->next = newnode;
	else
		(cacheptr->hashtable[hashindex]).first = newnode;
	newnode->prev = (cacheptr->hashtable[hashindex]).last;
	newnode->next = NULL;
	(cacheptr->hashtable[hashindex]).last = newnode;

	/* Change the global linked list */
	if (cacheptr->gfirst != NULL)
		(cacheptr->gfirst)->gprev = newnode;
	newnode->gnext = cacheptr->gfirst;
	newnode->gprev = NULL;
	cacheptr->gfirst = newnode;
	(cacheptr->num_nodes)++;
}

/* Helper function for repositioning the node according to usage */
void _node_reposition(PATH_LOOKUP *tmpptr, int32_t hashindex, PATH_CACHE *cacheptr)
{
	PATH_LOOKUP *tmpprev, *tmpgnext;
	while (tmpptr->prev != NULL) {
		if ((tmpptr->lookupcount) > ((tmpptr->prev)->lookupcount)) {
			/* Exchange tmpptr with tmpptr->prev */
			tmpprev = tmpptr->prev;
			if (tmpptr->next == NULL)
				(cacheptr->hashtable[hashindex]).last = tmpprev;
			else
				(tmpptr->next)->prev = tmpprev;
			if (tmpprev->prev == NULL)
				(cacheptr->hashtable[hashindex]).first = tmpptr;
			else
				(tmpprev->prev)->next = tmpptr;
			tmpptr->prev = tmpprev->prev;
			tmpprev->next = tmpptr->next;
			tmpptr->next = tmpprev;
			tmpprev->prev = tmpptr;
		} else {
			break;
		}
	}

	while (tmpptr->gnext != NULL) {
		if ((tmpptr->lookupcount) > ((tmpptr->gnext)->lookupcount)) {
			/* Exchange tmpptr with tmpptr->gnext */
			tmpgnext = tmpptr->gnext;
			if (tmpptr->gprev == NULL)
				cacheptr->gfirst = tmpgnext;
			else
				(tmpptr->gprev)->gnext = tmpgnext;
			if (tmpgnext->gnext != NULL)
				(tmpgnext->gnext)->gprev = tmpptr;
			tmpptr->gnext = tmpgnext->gnext;
			tmpgnext->gprev = tmpptr->gprev;
			tmpptr->gprev = tmpgnext;
			tmpgnext->gnext = tmpptr;
		} else {
			break;
		}
	}
}

int32_t search_inode(ino_t parent, ino_t child, DIR_ENTRY *dentry)
{
	META_CACHE_ENTRY_STRUCT *cache_entry;
	DIR_META_TYPE tempmeta;
	DIR_ENTRY_PAGE temp_page;
	int32_t ret, errcode;
	int32_t count;

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
int32_t lookup_name(PATH_CACHE *cacheptr, ino_t thisinode, PATH_LOOKUP *retnode)
{
	int32_t ret, errcode;
	int32_t hashindex;
	PATH_LOOKUP *tmpptr;
	ino_t parentinode, *parentlist;
	int32_t numparents;
	DIR_ENTRY tmpentry;

	hashindex = thisinode % NUM_LOOKUP_ENTRY;

	tmpptr = (cacheptr->hashtable[hashindex]).first;

	while (tmpptr != NULL) {
		if (thisinode == tmpptr->child) {
			/* We found the node. Update the lookup count
			and the position if needed, and return the result */
			memcpy(retnode, tmpptr, sizeof(PATH_LOOKUP));
			(tmpptr->lookupcount)++;
			_node_reposition(tmpptr, hashindex, cacheptr);
			return 0;
		}
		tmpptr = tmpptr->next;
	}
        ret = sem_wait(&(pathlookup_data_lock));
        if (ret < 0) {
                errcode = errno;
                write_log(0, "Unexpected error: %d (%s)\n", errcode,
                          strerror(errcode));
                errcode = -errcode;
                return errcode;
        }

	/* Did not find the node. Proceed to lookup and then add to cache */

        parentlist = NULL;
        ret = fetch_all_parents(thisinode, &numparents, &parentlist);
        if (ret < 0) {
                errcode = ret;
                goto errcode_handle;
        }

	/* If there is no parent, return error */
	if (numparents <= 0) {
		errcode = -ENOENT;
		write_log(10, "Cannot find parent in lookup\n");
		goto errcode_handle;
	}

	parentinode = parentlist[0];
	free(parentlist);
	parentlist = NULL;
	sem_post(&(pathlookup_data_lock));

	write_log(10, "Debug parent lookup %" PRIu64 " %" PRIu64 "\n",
	          (uint64_t) thisinode, (uint64_t) parentinode);
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
	memset(tmpptr, 0, sizeof(PATH_LOOKUP));
	write_log(10, "Sizeof path_lookup %d\n", sizeof(PATH_LOOKUP));
	tmpptr->self = tmpptr;
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
	sem_post(&(pathlookup_data_lock));
	if (parentlist != NULL)
		free(parentlist);
	return errcode;
}


/************************************************************************
*
* Function name: construct_path_iterate
*        Inputs: PATH_CACHE *cacheptr, ino_t thisinode, char **result,
*                int32_t bufsize
*       Summary: Recursively prepend the name of thisinode to the
*                pathname pointed by *result. bufsize indicates the
*                buffer size storing the pathname.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t construct_path_iterate(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
		int32_t bufsize)
{
	char *current_path, *tmpptr;
	PATH_LOOKUP cachenode;
	ino_t parent_inode;
	int32_t ret, pathlen, errcode;

	write_log(10, "Debug path iterate %" PRIu64 "\n",
		  (uint64_t) thisinode);

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

	if (pathlen > PATH_MAX) {
		errcode = -ENAMETOOLONG;
		write_log(0, "Unexpected error %d\n", ENAMETOOLONG);
		goto errcode_handle;
	}

	if ((*result)[0] == '\0')
		snprintf(current_path, pathlen, "%s", cachenode.childname);
	else
		snprintf(current_path, pathlen, "%s/%s",
			cachenode.childname, *result);

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
	if (tmpptr != NULL) {
		*result = NULL;
		free(tmpptr);
	}
	return errcode;
}

/************************************************************************
*
* Function name: construct_path
*        Inputs: MOUNT_T *tmpptr, ino_t thisinode, char **result
*       Summary: Construct the path from root to thisinode, and return
*                the path via the pointer in *result.
*          Note: The caller is responsible for freeing the memory storing
*                the result string.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t construct_path(PATH_CACHE *cacheptr, ino_t thisinode, char **result,
                   ino_t rootinode)
{
	int32_t ret, errcode;
	char *tmpbuf;
	char pathname[200];
	FILE *fptr;
	int64_t ret_pos;

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

	tmpbuf[0] = '\0';
	*result = tmpbuf;
	if (thisinode == cacheptr->root_inode) {
		snprintf(*result, 10, "/");
	} else {
		ret = construct_path_iterate(cacheptr, thisinode, result, 10);
		if ((ret < 0) && (ret != -ENOENT)) {
			errcode = ret;
			goto errcode_handle;
		}
		if (ret == -ENOENT) {
			snprintf(pathname, 200,
			         "%s/markdelete/inode%" PRIu64 "_%" PRIu64 "",
			         METAPATH, (uint64_t)thisinode,
			         (uint64_t)rootinode);
			if (access(pathname, F_OK) != 0) {
				errcode = -ENOENT;
				goto errcode_handle;
			}
			fptr = fopen(pathname, "r");
			FSEEK(fptr, 0, SEEK_END);
			FTELL(fptr);
			if (ret_pos == 0) {
				errcode = -ENOENT;
				fclose(fptr);
				goto errcode_handle;
			}
			free(*result);
			*result = NULL;
			*result = (char *) malloc(((size_t) ret_pos) + 10);
			if (*result == NULL) {
				errcode = -ENOMEM;
				write_log(0, "Out of memory\n");
				goto errcode_handle;
			}
			FSEEK(fptr, 0, SEEK_SET);
			fscanf(fptr, "%s ", *result);
			write_log(10, "Read path %s\n", *result);
			fclose(fptr);
		}
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
int32_t delete_pathcache_node(PATH_CACHE *cacheptr, ino_t todelete)
{
	int32_t ret, errcode;
	int32_t hashindex;
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

			memset(tmpptr, 0, sizeof(PATH_LOOKUP));
			free(tmpptr);
			(cacheptr->num_nodes)--;
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
int32_t init_pathlookup()
{
	int32_t ret, errcode;
	char pathname[METAPATHLEN+10];
	BOOL need_init;
	ssize_t ret_ssize;

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
		MKNOD(pathname, S_IFREG | 0600, 0);
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

	need_init = FALSE;
	snprintf(pathname, METAPATHLEN, "%s/parentlookup2_db", METAPATH);
	if (access(pathname, F_OK) != 0) {
		MKNOD(pathname, S_IFREG | 0600, 0);
		need_init = TRUE;
	}
	plookup2_fptr = fopen(pathname, "r+");
	if (plookup2_fptr == NULL) {
		errcode = errno;
		write_log(0, "Unexpected error in init: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		goto errcode_handle;
	}
	setbuf(plookup2_fptr, NULL);
	if (need_init == TRUE) {
		/* Init the parent lookup secondary db if just created */
		memset(&parent_lookup_head, 0, sizeof(PLOOKUP_HEAD_T));
		PWRITE(fileno(plookup2_fptr), &parent_lookup_head,
		       sizeof(PLOOKUP_HEAD_T), 0);
	} else {
		/* Otherwise read from database */
		PREAD(fileno(plookup2_fptr), &parent_lookup_head,
		       sizeof(PLOOKUP_HEAD_T), 0);
	}

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
	fclose(plookup2_fptr);
	sem_destroy(&(pathlookup_data_lock));
	return;
}

/************************************************************************
*
* Function name: pathlookup_write_parent
*        Inputs: ino_t self_inode, ino_t parent_inode
*       Summary: Writes the parent inode to a file in metastorage, and reset
*                haveothers to FALSE.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t pathlookup_write_parent(ino_t self_inode, ino_t parent_inode)
{
	off_t filepos;
	PRIMARY_PARENT_T tmpparent;
	int32_t errcode, ret;
	ssize_t ret_ssize;

	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		return errcode;
	}

	filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
	tmpparent.parentinode = parent_inode;
	tmpparent.haveothers = FALSE;
	PWRITE(fileno(pathlookup_data_fptr), &tmpparent,
	       sizeof(PRIMARY_PARENT_T), filepos);

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
int32_t pathlookup_read_parent(ino_t self_inode, ino_t *parentptr)
{
	off_t filepos;
	PRIMARY_PARENT_T tmpparent;
	int32_t errcode, ret;
	ssize_t ret_ssize;

	ret = sem_wait(&(pathlookup_data_lock));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Unexpected error: %d (%s)\n", errcode,
			strerror(errcode));
		errcode = -errcode;
		return errcode;
	}

	filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
	memset(&tmpparent, 0, sizeof(PRIMARY_PARENT_T));
	PREAD(fileno(pathlookup_data_fptr), &tmpparent,
	       sizeof(PRIMARY_PARENT_T), filepos);
	*parentptr = tmpparent.parentinode;
	sem_post(&(pathlookup_data_lock));
	return 0;
errcode_handle:
	sem_post(&(pathlookup_data_lock));
	return errcode;
}


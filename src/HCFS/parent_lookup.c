/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "parent_lookup.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "logger.h"
#include "meta_mem_cache.h"
#include "fuseop.h"
#include "macro.h"
#include "path_reconstruct.h"
#include "global.h"
#include "utils.h"

/************************************************************************
*
* Function name: fetch_all_parents
*        Inputs: ino_t self_inode, int32_t *parentnum, ino_t **parentlist
*       Summary: Returns the list of parents for "self_inode" in "parentlist".
*                The number of parents is returned in "parentnum".
*                When calling the function, "*parentlist" should be NULL, and
*                after the function returns, the caller should free
*                "parentlist".
*                pathlookup_data_lock should be locked before calling.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t fetch_all_parents(ino_t self_inode, int32_t *parentnum, ino_t **parentlist)
{
	int32_t totalnum;
	PRIMARY_PARENT_T tmpparent;
	int32_t sem_val;
	off_t filepos;
	int32_t errcode, ret;
	ssize_t ret_ssize;
	int32_t hashval, tmpmaxnum;
	PLOOKUP_PAGE_T tmppage;
	int64_t tmppos;
	uint8_t count;
	ino_t *tmpptr;

	if (self_inode <= 0)
		return -EINVAL;

	ret = sem_getvalue(&(pathlookup_data_lock), &sem_val);
	if ((sem_val > 0) || (ret < 0))
		return -EINVAL;

	filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
	memset(&tmpparent, 0, sizeof(PRIMARY_PARENT_T));
	ret_ssize = PREAD(fileno(pathlookup_data_fptr), &tmpparent,
	      sizeof(PRIMARY_PARENT_T), filepos);

	if (ret_ssize != sizeof(PRIMARY_PARENT_T)) {
		write_log(2, "Short-read when querying parent db. (Inode %"
		          PRIu64 ")\n", (uint64_t) self_inode);
		*parentnum = 0;
		return 0;
	}

	if ((tmpparent.parentinode == 0) && (tmpparent.haveothers == FALSE)) {
		*parentnum = 0;
		return 0;
	}
	totalnum = 0;

	*parentlist = (ino_t *) calloc(10, sizeof(ino_t));
	if (*parentlist == NULL)
		return -ENOMEM;

	tmpmaxnum = 10;

	/* If primary lookup is not empty */
	if (tmpparent.parentinode > 0) {
		memcpy(&((*parentlist)[totalnum]), &(tmpparent.parentinode),
		       sizeof(ino_t));
		totalnum++;
	}
	if (tmpparent.haveothers == FALSE) {
		*parentnum = totalnum;
		return 0;
	}

	hashval = self_inode % PLOOKUP_HASH_NUM_ENTRIES;

	tmppos = parent_lookup_head.hash_head[hashval];
	while (tmppos != 0) {
		PREAD(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
		      tmppos);
		if (tmppage.thisinode == self_inode)
			break;
		tmppos = tmppage.nextlookup;
	}
	if (tmppos == 0) {  /* If cannot find entry in hash table */
		*parentnum = totalnum;
		return 0;
	}

	/* Starts reading the parents from here */
	while (tmppos != 0) {
		for (count = 0; count < tmppage.num_parents; count++) {
			memcpy(&((*parentlist)[totalnum]),
			       &(tmppage.parents[count]), sizeof(ino_t));
			totalnum++;
			if (totalnum >= tmpmaxnum) {
				tmpmaxnum += 10;
				tmpptr = (ino_t *) realloc(*parentlist,
			                      tmpmaxnum * sizeof(ino_t));
				if (tmpptr == NULL) {
					/* Cannot allocate more memory */
					free(*parentlist);
					*parentlist = NULL;
					errcode = -ENOMEM;
					goto errcode_handle;
				}
				*parentlist = tmpptr;
			}
		}
		tmppos = tmppage.nextpage;
		if (tmppos != 0) {
			/* Read the next page in the same inode */
			PREAD(fileno(plookup2_fptr), &tmppage,
			      sizeof(PLOOKUP_PAGE_T), tmppos);
		}
	}
	*parentnum = totalnum;

	return 0;
errcode_handle:
	return errcode;
}

/* Helper function for allocating a new lookup page */
static inline int64_t _alloc_new_page(int32_t *reterr)
{
	int64_t ret_pos, tmppos;
	PLOOKUP_PAGE_T tmppage;

	if (parent_lookup_head.gc_head != 0) {
		tmppos = parent_lookup_head.gc_head;
		PREAD(fileno(plookup2_fptr), &tmppage,
		      sizeof(PLOOKUP_PAGE_T), tmppos);
		parent_lookup_head.gc_head = tmppage.gc_next;
		PWRITE(fileno(plookup2_fptr), &parent_lookup_head,
		       sizeof(PLOOKUP_HEAD_T), 0);
	} else {
		FSEEK(plookup2_fptr, 0, SEEK_END);
		ret_pos = FTELL(plookup2_fptr);
		tmppos = ret_pos;
	}
	*reterr = 0;
	return tmppos;
errcode_handle:
	*reterr = errcode;
	return 0;
}
/* Helper function for adding a new page for a new inode to the head */
static inline int32_t _add_to_head(ino_t self_inode, ino_t parent_inode)
{
	int32_t hashval;
	PLOOKUP_PAGE_T tmppage;
	int64_t tmppos;
	int32_t errcode;

	hashval = self_inode % PLOOKUP_HASH_NUM_ENTRIES;
	tmppos = _alloc_new_page(&errcode);
	if (tmppos == 0) {
		if (errcode >= 0)
			errcode = -EIO;
		goto errcode_handle;
	}
	parent_lookup_head.hash_head[hashval] = tmppos;
	memset(&tmppage, 0, sizeof(PLOOKUP_PAGE_T));
	tmppage.thisinode = self_inode;
	tmppage.num_parents = 1;
	tmppage.parents[0] = parent_inode;
	PWRITE(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
	       tmppos);
	PWRITE(fileno(plookup2_fptr), &parent_lookup_head,
	       sizeof(PLOOKUP_HEAD_T), 0);
	return 0;
errcode_handle:
	return errcode;
}

/* Helper function for adding a new inode to the end of the hash linked
list */
static inline int32_t _add_to_tail(ino_t self_inode, ino_t parent_inode,
                               int64_t prevpos)
{
	PLOOKUP_PAGE_T tmppage;
	int64_t tmppos;
	int32_t errcode;

	tmppos = _alloc_new_page(&errcode);
	if (tmppos == 0) {
		if (errcode >= 0)
			errcode = -EIO;
		goto errcode_handle;
	}

	memset(&tmppage, 0, sizeof(PLOOKUP_PAGE_T));
	tmppage.thisinode = self_inode;
	tmppage.num_parents = 1;
	tmppage.parents[0] = parent_inode;
	PWRITE(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
	       tmppos);

	/* Write link update to the previous page */
	PREAD(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
	       prevpos);
	tmppage.nextlookup = tmppos;
	PWRITE(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
	       prevpos);

	return 0;
errcode_handle:
	return errcode;
}

/* Helper function for adding a new page to the end of the page linked
list for the same inode */
static inline int32_t _add_new_page(ino_t self_inode, ino_t parent_inode,
                               int64_t prevpos)
{
	PLOOKUP_PAGE_T tmppage;
	int64_t tmppos;
	int32_t errcode;

	tmppos = _alloc_new_page(&errcode);
	if (tmppos == 0) {
		if (errcode >= 0)
			errcode = -EIO;
		goto errcode_handle;
	}

	memset(&tmppage, 0, sizeof(PLOOKUP_PAGE_T));
	tmppage.thisinode = self_inode;
	tmppage.num_parents = 1;
	tmppage.parents[0] = parent_inode;
	PWRITE(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
	       tmppos);

	/* Write link update to the previous page */
	PREAD(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
	       prevpos);
	tmppage.nextpage = tmppos;
	PWRITE(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
	       prevpos);

	return 0;
errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: lookup_add_parent
*        Inputs: ino_t self_inode, ino_t parent_inode
*       Summary: Add a parent "parent_inode" to the parent lookup for
*                "self_inode".
*                pathlookup_data_lock should be locked before calling.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t lookup_add_parent(ino_t self_inode, ino_t parent_inode)
{
	off_t filepos;
	int32_t ret;
	int32_t sem_val;
	PRIMARY_PARENT_T tmpparent;
	int32_t hashval;
	PLOOKUP_PAGE_T tmppage;
	int64_t tmppos, prevpos;

	if ((self_inode <= 0) || (parent_inode <= 0))
		return -EINVAL;

	ret = sem_getvalue(&(pathlookup_data_lock), &sem_val);
	if ((sem_val > 0) || (ret < 0))
		return -EINVAL;

	memset(&tmpparent, 0, sizeof(PRIMARY_PARENT_T));
	filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
	PREAD(fileno(pathlookup_data_fptr), &tmpparent,
	      sizeof(PRIMARY_PARENT_T), filepos);

	if (tmpparent.parentinode == 0) {
		/* Add the parent to the primary lookup db */
		tmpparent.parentinode = parent_inode;
		PWRITE(fileno(pathlookup_data_fptr), &tmpparent,
		       sizeof(PRIMARY_PARENT_T), filepos);
		return 0;
	}
	/* Need to find an entry in the secondary lookup */
	if (tmpparent.haveothers == FALSE) {
		tmpparent.haveothers = TRUE;
		PWRITE(fileno(pathlookup_data_fptr), &tmpparent,
		       sizeof(PRIMARY_PARENT_T), filepos);
	}

	hashval = self_inode % PLOOKUP_HASH_NUM_ENTRIES;
	tmppos = parent_lookup_head.hash_head[hashval];
	if (tmppos == 0) {
		/* Add a new page to the hash head */
		ret = _add_to_head(self_inode, parent_inode);
		return ret;
	}

	prevpos = 0;
	while (tmppos != 0) {
		PREAD(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
		      tmppos);
		if (tmppage.thisinode == self_inode)
			break;
		prevpos = tmppos;
		tmppos = tmppage.nextlookup;
	}

	if (tmppos == 0) {
		/* Add a new page to the tail of the linked list */
		ret = _add_to_tail(self_inode, parent_inode, prevpos);
		return ret;
	}

	/* Check for empty slot for the new parent */
	prevpos = 0;
	while (tmppos != 0) {
		if (tmppage.num_parents < MAX_PARENTS_PER_PAGE) {
			tmppage.parents[tmppage.num_parents] = parent_inode;
			tmppage.num_parents++;
			PWRITE(fileno(plookup2_fptr), &tmppage,
			      sizeof(PLOOKUP_PAGE_T), tmppos);
			return 0;
		}
		prevpos = tmppos;
		tmppos = tmppage.nextpage;
		if (tmppos != 0) {
			/* Read the next page in the same inode */
			PREAD(fileno(plookup2_fptr), &tmppage,
			      sizeof(PLOOKUP_PAGE_T), tmppos);
		}
	}
	/* No empty slot for the existing pages. Allocate a new one */

	ret = _add_new_page(self_inode, parent_inode, prevpos);
	return ret;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: lookup_delete_parent
*        Inputs: ino_t self_inode, ino_t parent_inode
*       Summary: Deletes a parent "parent_inode" from the parent lookup for
*                "self_inode".
*                pathlookup_data_lock should be locked before calling.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t lookup_delete_parent(ino_t self_inode, ino_t parent_inode)
{
	off_t filepos;
	int32_t ret;
	int32_t sem_val;
	int64_t prevllpos, prevpagepos, tmppos, tmpnextpos;
	PRIMARY_PARENT_T tmpparent;
	int32_t hashval;
	PLOOKUP_PAGE_T tmppage, prevpage, nextpage;
	uint8_t deleteall;
	int8_t count;

	if ((self_inode <= 0) || (parent_inode <= 0))
		return -EINVAL;

	ret = sem_getvalue(&(pathlookup_data_lock), &sem_val);
	if ((sem_val > 0) || (ret < 0))
		return -EINVAL;

	filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
	PREAD(fileno(pathlookup_data_fptr), &tmpparent,
	      sizeof(PRIMARY_PARENT_T), filepos);

	if (tmpparent.parentinode == parent_inode) {
		/* Remove the parent from the primary lookup db */
		tmpparent.parentinode = 0;
		PWRITE(fileno(pathlookup_data_fptr), &tmpparent,
		       sizeof(PRIMARY_PARENT_T), filepos);
		return 0;
	}

	if (tmpparent.haveothers == FALSE)
		return -ENOENT;

	hashval = self_inode % PLOOKUP_HASH_NUM_ENTRIES;
	tmppos = parent_lookup_head.hash_head[hashval];

	/* prevllpos is kept so that the entire inode can be removed
	if empty */
	prevllpos = 0;
	while (tmppos != 0) {
		PREAD(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
		      tmppos);
		if (tmppage.thisinode == self_inode)
			break;
		prevllpos = tmppos;
		tmppos = tmppage.nextlookup;
	}
	if (tmppos == 0) {  /* If cannot find entry in hash table */
		/* Flag in primary lookup db is not valid. Reset. */
		filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
		PREAD(fileno(pathlookup_data_fptr), &tmpparent,
		      sizeof(PRIMARY_PARENT_T), filepos);
		tmpparent.haveothers = FALSE;
		PWRITE(fileno(pathlookup_data_fptr), &tmpparent,
		      sizeof(PRIMARY_PARENT_T), filepos);
		return -ENOENT;
	}

	/* Search the page list for the inode found */
	/* Starts reading the parents from here */
	prevpagepos = 0;
	while (tmppos != 0) {
		for (count = 0; count < tmppage.num_parents; count++) {
			if (tmppage.parents[count] == parent_inode)
				break;
		}
		if (count < tmppage.num_parents) /* If found */
			break;
		prevpagepos = tmppos; /* This is for tracking linked list */
		tmppos = tmppage.nextpage;
		if (tmppos != 0) {
			/* Read the next page in the same inode */
			PREAD(fileno(plookup2_fptr), &tmppage,
			      sizeof(PLOOKUP_PAGE_T), tmppos);
		}
	}

	/* Did not find the parent to be replaced */
	if (tmppos == 0)
		return -ENOENT;

	/* Found the entry. Need to delete */
	tmppage.num_parents--;
	if (tmppage.num_parents > 0) {
		/* If there are other parents in this page */
		if ((tmppage.num_parents - count) > 0)
			memmove(&(tmppage.parents[count]),
			        &(tmppage.parents[count+1]),
			        sizeof(ino_t) * (tmppage.num_parents - count));
		PWRITE(fileno(plookup2_fptr), &tmppage,
		       sizeof(PLOOKUP_PAGE_T), tmppos);
		return 0;
	}

	/* There is nothing left in this page. Delete and put to gc */
	tmpnextpos = tmppage.nextpage;
	if ((prevpagepos == 0) && (tmpnextpos == 0)) {
		/* Nothing left for this inode */
		deleteall = TRUE;
		tmpnextpos = tmppage.nextlookup;
	} else {
		deleteall = FALSE;
	}

	if (deleteall == FALSE) {
		if (prevpagepos != 0) {
			PREAD(fileno(plookup2_fptr), &prevpage,
			      sizeof(PLOOKUP_PAGE_T), prevpagepos);
			prevpage.nextpage = tmpnextpos;
			PWRITE(fileno(plookup2_fptr), &prevpage,
			       sizeof(PLOOKUP_PAGE_T), prevpagepos);
		} else if (prevllpos == 0) {
			/* The next page is the new head. Need to
			change the linked list for the hash */
			/* Here the new head is also the first in the
			hash linked list */
			PREAD(fileno(plookup2_fptr), &nextpage,
			      sizeof(PLOOKUP_PAGE_T), tmpnextpos);
			nextpage.nextlookup = tmppage.nextlookup;
			PWRITE(fileno(plookup2_fptr), &nextpage,
			       sizeof(PLOOKUP_PAGE_T), tmpnextpos);
			parent_lookup_head.hash_head[hashval] = tmpnextpos;
			PWRITE(fileno(plookup2_fptr), &parent_lookup_head,
			       sizeof(PLOOKUP_HEAD_T), 0);
		} else {
			/* The next page is the new head. Need to
			change the linked list for the hash */
			/* Here also need to update the link in the previous
			inode on the hash linked list */
			PREAD(fileno(plookup2_fptr), &nextpage,
			      sizeof(PLOOKUP_PAGE_T), tmpnextpos);
			nextpage.nextlookup = tmppage.nextlookup;
			PWRITE(fileno(plookup2_fptr), &nextpage,
			       sizeof(PLOOKUP_PAGE_T), tmpnextpos);
			PREAD(fileno(plookup2_fptr), &prevpage,
			      sizeof(PLOOKUP_PAGE_T), prevllpos);
			prevpage.nextlookup = tmpnextpos;
			PWRITE(fileno(plookup2_fptr), &prevpage,
			      sizeof(PLOOKUP_PAGE_T), prevllpos);
		}
	} else {
		/* If need to delete the entire inode from the secondary db */
		if (prevllpos == 0) {
			parent_lookup_head.hash_head[hashval] = tmpnextpos;
			PWRITE(fileno(plookup2_fptr), &parent_lookup_head,
			       sizeof(PLOOKUP_HEAD_T), 0);
		} else {
			PREAD(fileno(plookup2_fptr), &prevpage,
			      sizeof(PLOOKUP_PAGE_T), prevllpos);
			prevpage.nextlookup = tmpnextpos;
			PWRITE(fileno(plookup2_fptr), &prevpage,
			      sizeof(PLOOKUP_PAGE_T), prevllpos);
		}
		/* Mark haveothers as FALSE in the primary lookup db */
		filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
		PREAD(fileno(pathlookup_data_fptr), &tmpparent,
		      sizeof(PRIMARY_PARENT_T), filepos);
		tmpparent.haveothers = FALSE;
		PWRITE(fileno(pathlookup_data_fptr), &tmpparent,
		      sizeof(PRIMARY_PARENT_T), filepos);
	}

	/* Recycle the old page */
	memset(&tmppage, 0, sizeof(PLOOKUP_PAGE_T));
	tmppage.gc_next = parent_lookup_head.gc_head;
	parent_lookup_head.gc_head = tmppos;
	PWRITE(fileno(plookup2_fptr), &tmppage,
		       sizeof(PLOOKUP_PAGE_T), tmppos);
	PWRITE(fileno(plookup2_fptr), &parent_lookup_head,
		       sizeof(PLOOKUP_HEAD_T), 0);

	return 0;

errcode_handle:
	return errcode;
}

/************************************************************************
*
* Function name: lookup_replace_parent
*        Inputs: ino_t self_inode, ino_t parent_inode1, ino_t parent_inode2
*       Summary: Replace a parent "parent_inode1" with "parent_inode2" in the *                parent lookup for "self_inode".
*                pathlookup_data_lock should be locked before calling.
*  Return value: 0 if successful. Otherwise returns negation of error code.
*
*************************************************************************/
int32_t lookup_replace_parent(ino_t self_inode, ino_t parent_inode1,
			  ino_t parent_inode2)
{
	off_t filepos;
	int32_t ret;
	int32_t sem_val;
	PRIMARY_PARENT_T tmpparent;
	int32_t hashval;
	PLOOKUP_PAGE_T tmppage;
	int64_t tmppos;
	uint8_t count;

	if (((self_inode <= 0) || (parent_inode1 <= 0)) || (parent_inode2 <= 0))
		return -EINVAL;

	ret = sem_getvalue(&(pathlookup_data_lock), &sem_val);
	if ((sem_val > 0) || (ret < 0))
		return -EINVAL;

	filepos = (off_t) ((self_inode - 1) * sizeof(PRIMARY_PARENT_T));
	PREAD(fileno(pathlookup_data_fptr), &tmpparent,
	      sizeof(PRIMARY_PARENT_T), filepos);

	if (tmpparent.parentinode == parent_inode1) {
		/* Replace the parent */
		tmpparent.parentinode = parent_inode2;
		PWRITE(fileno(pathlookup_data_fptr), &tmpparent,
		       sizeof(PRIMARY_PARENT_T), filepos);
		return 0;
	}

	if (tmpparent.haveothers == FALSE)
		return -ENOENT;

	hashval = self_inode % PLOOKUP_HASH_NUM_ENTRIES;
	tmppos = parent_lookup_head.hash_head[hashval];
	while (tmppos != 0) {
		PREAD(fileno(plookup2_fptr), &tmppage, sizeof(PLOOKUP_PAGE_T),
		      tmppos);
		if (tmppage.thisinode == self_inode)
			break;
		tmppos = tmppage.nextlookup;
	}
	if (tmppos == 0)  /* If cannot find entry in hash table */
		return -ENOENT;

	/* Starts reading the parents from here */
	while (tmppos != 0) {
		for (count = 0; count < tmppage.num_parents; count++) {
			if (tmppage.parents[count] == parent_inode1) {
				tmppage.parents[count] = parent_inode2;
				PWRITE(fileno(plookup2_fptr), &tmppage,
				      sizeof(PLOOKUP_PAGE_T), tmppos);
				return 0;
			}
		}
		tmppos = tmppage.nextpage;
		if (tmppos != 0) {
			/* Read the next page in the same inode */
			PREAD(fileno(plookup2_fptr), &tmppage,
			      sizeof(PLOOKUP_PAGE_T), tmppos);
		}
	}

	/* Did not find the parent to be replaced */
	return -ENOENT;
errcode_handle:
	return errcode;
}


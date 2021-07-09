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

/* HOWTO: 1. in lookup_count, add a field "to_delete". rmdir, unlink
* will first mark this as true and if in forget() the count is dropped
* to zero, the inode is deleted.
*          2. to allow inode deletion fixes due to system crashing, a subfolder
* will be created so that the inode number of inodes to be deleted can be
* touched here, and removed when actually deleted.
*           3. in lookup_decrease, should delete nodes when lookup drops
* to zero (to save space in the long run).
*           4. in unmount, can pick either scanning lookup table for inodes
* to delete or list the folder.
*/

#include "lookup_count.h"

#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "fuseop.h"
#include "global.h"
#include "metaops.h"
#include "logger.h"

/************************************************************************
*
* Function name: lookup_init
*        Inputs: Array of lookup head "lookup_table"
*        Output: 0 if successful, otherwise negation of error code.
*       Summary: Initialize the inode lookup count table
*
*************************************************************************/

int32_t lookup_init(LOOKUP_HEAD_TYPE *lookup_table)
{
	int32_t count;
	int32_t ret_val, errcode;

	if (lookup_table == NULL)
		return -ENOMEM;

	for (count = 0; count < NUM_LOOKUP_ENTRIES; count++) {
		ret_val = sem_init(&(lookup_table[count].entry_sem), 0, 1);
		if (ret_val < 0) {
			errcode = errno;
			return -errcode;
		}
		lookup_table[count].head = NULL;
	}

	return 0;
}

/************************************************************************
*
* Function name: lookup_increase
*        Inputs: LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode,
*                int32_t amount, char d_type
*        Output: The updated lookup count if successful, or negation of
*                error code if not.
*       Summary: Increase the inode lookup count for this_inode, creating
*                an entry in the table if necessary.
*
*************************************************************************/

int32_t lookup_increase(LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode,
				int32_t amount, char d_type)
{
	int32_t index;
	int32_t ret_val, errcode;
	char found;
	LOOKUP_NODE_TYPE *ptr;

	write_log(10, "Debug lookup increase for inode %" PRIu64 ", amount %d\n",
			(uint64_t)this_inode, amount);

	if (lookup_table == NULL)
		return -ENOMEM;

	index = this_inode % NUM_LOOKUP_ENTRIES;

	ret_val = sem_wait(&(lookup_table[index].entry_sem));

	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n", __func__, errcode,
			strerror(errcode));
		return -errcode;
	}

	ptr = lookup_table[index].head;
	found = FALSE;

	while (ptr != NULL) {
		if (ptr->this_inode == this_inode) {
			found = TRUE;
			ptr->lookup_count += amount;
			break;
		}
		ptr = ptr->next;
	}

	if (found == FALSE) {  /* Will need to create a new node */
		ptr = malloc(sizeof(LOOKUP_NODE_TYPE));
		if (ptr == NULL) {
			write_log(0, "Out of memory in %s\n", __func__);
			sem_post(&(lookup_table[index].entry_sem));
			return -ENOMEM;
		}
		memset(ptr, 0, sizeof(LOOKUP_NODE_TYPE));
		ptr->this_inode = this_inode;
		ptr->lookup_count = amount;
		ptr->to_delete = FALSE;
		ptr->d_type = d_type;
		ptr->next = lookup_table[index].head;
		lookup_table[index].head = ptr;
	}

	write_log(10, "Debug lookup increase lookup now %d\n",
				ptr->lookup_count);

	ret_val = sem_post(&(lookup_table[index].entry_sem));

	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n", __func__, errcode,
			strerror(errcode));
		return -errcode;
	}

	return ptr->lookup_count;
}

/************************************************************************
*
* Function name: lookup_decrease
*        Inputs: LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode,
*                int32_t amount, char *d_type, char *need_delete
*        Output: The updated lookup count if successful, or negation of error
*                code if not.
*       Summary: Decrease the inode lookup count for this_inode. If lookup
*                count is dropped to zero, remove the node. If the inode
*                needs to be deleted when lookup is zero, mark in *need_delete.
*
*************************************************************************/

int32_t lookup_decrease(LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode,
			int32_t amount, char *d_type, char *need_delete)
{
	int32_t index;
	int32_t ret_val, result_lookup, errcode;
	char found;
	LOOKUP_NODE_TYPE *ptr, *prev_ptr;

	write_log(10, "Debug lookup decrease for inode %" PRIu64 ", amount %d\n",
			(uint64_t)this_inode, amount);

	if (lookup_table == NULL)
		return -ENOMEM;

	if (need_delete == NULL)
		return -EPERM;

	*need_delete = FALSE;
	index = this_inode % NUM_LOOKUP_ENTRIES;

	ret_val = sem_wait(&(lookup_table[index].entry_sem));

	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n", __func__, errcode,
			strerror(errcode));
		return -errcode;
	}

	ptr = lookup_table[index].head;
	prev_ptr = NULL;
	found = FALSE;

	while (ptr != NULL) {
		if (ptr->this_inode == this_inode) {
			found = TRUE;
			ptr->lookup_count -= amount;
			if (ptr->lookup_count < 0) {
				write_log(5,
					"Debug lookup underflow. Resetting\n");
				ptr->lookup_count = 0;
			}
			result_lookup = ptr->lookup_count;
			*need_delete = ptr->to_delete;
			*d_type = ptr->d_type;
			/* Delete node if lookup_count is now zero */
			if (result_lookup == 0) {
				if (prev_ptr == NULL)
					lookup_table[index].head = ptr->next;
				else
					prev_ptr->next = ptr->next;
				free(ptr);
			}
			break;
		}
		prev_ptr = ptr;
		ptr = ptr->next;
	}

	if (found == FALSE) {
		write_log(5, "Debug no lookup value\n");
		result_lookup = -EINVAL;
		sem_post(&(lookup_table[index].entry_sem));
		return result_lookup;
	}

	write_log(10, "Debug lookup decrease lookup now %d\n", result_lookup);

	ret_val = sem_post(&(lookup_table[index].entry_sem));

	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n", __func__, errcode,
			strerror(errcode));
		return -errcode;
	}

	return result_lookup;
}

/************************************************************************
*
* Function name: lookup_markdelete
*        Inputs: LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode
*        Output: 0 if successful, or negation of error code if not.
*       Summary: Mark inode "this_inode" as to_delete.
*
*************************************************************************/

int32_t lookup_markdelete(LOOKUP_HEAD_TYPE *lookup_table, ino_t this_inode)
{
	int32_t index;
	int32_t ret_val, result_lookup, errcode;
	char found;
	LOOKUP_NODE_TYPE *ptr;

	write_log(10, "Debug lookup markdelete for inode %" PRIu64 "\n",
			(uint64_t)this_inode);

	if (lookup_table == NULL)
		return -ENOMEM;

	index = this_inode % NUM_LOOKUP_ENTRIES;

	ret_val = sem_wait(&(lookup_table[index].entry_sem));

	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n", __func__, errcode,
			strerror(errcode));
		return -errcode;
	}

	ptr = lookup_table[index].head;
	found = FALSE;

	while (ptr != NULL) {
		if (ptr->this_inode == this_inode) {
			found = TRUE;
			ptr->to_delete = TRUE;
			break;
		}
		ptr = ptr->next;
	}

	if (found == FALSE) {
		write_log(5, "Debug no lookup value\n");
		ret_val = sem_post(&(lookup_table[index].entry_sem));
		if (ret_val < 0) {  /* Unlock */
			errcode = errno;
			write_log(0, "Error in %s. Code %d, %s\n", __func__,
				errcode, strerror(errcode));
		}
		result_lookup = -EINVAL;
		return result_lookup;
	}

	ret_val = sem_post(&(lookup_table[index].entry_sem));

	if (ret_val < 0) {
		errcode = errno;
		write_log(0, "Error in %s. Code %d, %s\n", __func__, errcode,
			strerror(errcode));
		return -errcode;
	}

	return 0;
}

/************************************************************************
*
* Function name: lookup_destroy
*        Inputs: LOOKUP_HEAD_TYPE *lookup_table, MOUNT_T *tmpptr
*        Output: 0 if successful, otherwise -1.
*       Summary: Destroys the lookup count table, and delete inodes if
*                needed.
*
*************************************************************************/

int32_t lookup_destroy(LOOKUP_HEAD_TYPE *lookup_table, MOUNT_T *tmpptr)
{
	int32_t count;
	int32_t ret_val, errcode;
	LOOKUP_NODE_TYPE *ptr, *oldptr;

	write_log(10, "Debug lookup destroy\n");
	if (lookup_table == NULL)
		return -ENOMEM;

	for (count = 0; count < NUM_LOOKUP_ENTRIES; count++) {
		ret_val = sem_wait(&(lookup_table[count].entry_sem));

		if (ret_val < 0) {
			errcode = errno;
			write_log(0,
				"Error in %s. Code %d, %s\n", __func__, errcode,
				strerror(errcode));
			return -errcode;
		}

		ptr = lookup_table[count].head;

		while (ptr != NULL) {
			write_log(10, "Debug check delete %" PRIu64 "\n",
				(uint64_t)ptr->this_inode);
			ret_val = disk_checkdelete(ptr->this_inode,
						tmpptr->f_ino);

			if (ret_val == 1)
				actual_delete_inode(ptr->this_inode,
					ptr->d_type, tmpptr->f_ino, tmpptr);

			oldptr = ptr;
			ptr = ptr->next;
			free(oldptr);
		}

		lookup_table[count].head = NULL;

		ret_val = sem_post(&(lookup_table[count].entry_sem));

		if (ret_val < 0) {
			errcode = errno;
			write_log(0,
				"Error in %s. Code %d, %s\n", __func__, errcode,
				strerror(errcode));
			return -errcode;
		}
	}

	return 0;
}


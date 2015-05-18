/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: lookup_count.c
* Abstract: The c source code file for the lookup count table (for inodes).
*
* Revision History
* 2015/5/15 Jiahong created the file.
*
**************************************************************************/

/* HOWTO: 1. in lookup_count, add a field "to_delete". rmdir, unlink
will first mark this as true and if in forget() the count is dropped
to zero, the inode is deleted.
          2. to allow inode deletion fixes due to system crashing, a subfolder
will be created so that the inode number of inodes to be deleted can be
touched here, and removed when actually deleted.
          3. in lookup_decrease, should delete nodes when lookup drops
to zero (to save space in the long run).
          4. in unmount, can pick either scanning lookup table for inodes
to delete or list the folder.
*/

#include "lookup_count.h"

#include "fuseop.h"
#include "global.h"
#include "metaops.h"

/************************************************************************
*
* Function name: lookup_init
*        Inputs: None
*        Output: 0 if successful, otherwise -1.
*       Summary: Initialize the inode lookup count table
*
*************************************************************************/

int lookup_init()
{
	int count;
	int ret_val;

	for (count = 0; count < NUM_LOOKUP_ENTRIES; count++) {
		ret_val = sem_init(&(lookup_table[count].entry_sem), 0, 1);
		if (ret_val < 0)
			return ret_val;
		lookup_table[count].head = NULL;
	}

	return 0;
}

/************************************************************************
*
* Function name: lookup_increase
*        Inputs: ino_t this_inode, int amount
*        Output: The updated lookup count if successful, or -1 if not.
*       Summary: Increase the inode lookup count for this_inode, creating
*                an entry in the table if necessary.
*
*************************************************************************/

int lookup_increase(ino_t this_inode, int amount, char d_type)
{
	int index;
	int ret_val;
	char found;
	LOOKUP_NODE_TYPE *ptr;

	printf("Debug lookup increase for inode %lld, amount %d\n",
			this_inode, amount);
	index = this_inode % NUM_LOOKUP_ENTRIES;

	ret_val = sem_wait(&(lookup_table[index].entry_sem));

	if (ret_val < 0)
		return ret_val;

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
		ptr->this_inode = this_inode;
		ptr->lookup_count = amount;
		ptr->to_delete = FALSE;
		ptr->d_type = d_type;
		ptr->next = lookup_table[index].head;
		lookup_table[index].head = ptr;
	}

	printf("Debug lookup increase lookup now %d\n", ptr->lookup_count);

	ret_val = sem_post(&(lookup_table[index].entry_sem));

	if (ret_val < 0)
		return ret_val;

	return ptr->lookup_count;
}

/************************************************************************
*
* Function name: lookup_decrease
*        Inputs: ino_t this_inode, int amount, char *dtype, char *need_delete
*        Output: The updated lookup count if successful, or -1 if not.
*       Summary: Decrease the inode lookup count for this_inode. If lookup
*                count is dropped to zero, remove the node. If the inode
*                needs to be deleted when lookup is zero, mark in *need_delete.
*
*************************************************************************/

int lookup_decrease(ino_t this_inode, int amount,
			char *d_type, char *need_delete)
{
	int index;
	int ret_val, result_lookup;
	char found;
	LOOKUP_NODE_TYPE *ptr, *prev_ptr;

	printf("Debug lookup decrease for inode %lld, amount %d\n",
			this_inode, amount);

	if (need_delete == NULL)
		return -1;

	*need_delete = FALSE;
	index = this_inode % NUM_LOOKUP_ENTRIES;

	ret_val = sem_wait(&(lookup_table[index].entry_sem));

	if (ret_val < 0)
		return ret_val;

	ptr = lookup_table[index].head;
	prev_ptr = NULL;
	found = FALSE;

	while (ptr != NULL) {
		if (ptr->this_inode == this_inode) {
			found = TRUE;
			ptr->lookup_count -= amount;
			if (ptr->lookup_count < 0) {
				printf("Debug lookup underflow. Resetting\n");
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
		printf("Debug no lookup value\n");
		result_lookup = -1;
		sem_post(&(lookup_table[index].entry_sem));
		return result_lookup;
	}

	printf("Debug lookup decrease lookup now %d\n", result_lookup);

	ret_val = sem_post(&(lookup_table[index].entry_sem));

	if (ret_val < 0)
		return ret_val;

	return result_lookup;
}

/************************************************************************
*
* Function name: lookup_markdelete
*        Inputs: ino_t this_inode
*        Output: 0 if successful, or -1 if not.
*       Summary: Mark inode "this_inode" as to_delete.
*
*************************************************************************/

int lookup_markdelete(ino_t this_inode)
{
	int index;
	int ret_val, result_lookup;
	char found;
	LOOKUP_NODE_TYPE *ptr;

	printf("Debug lookup markdelete for inode %lld\n",
			this_inode);

	index = this_inode % NUM_LOOKUP_ENTRIES;

	ret_val = sem_wait(&(lookup_table[index].entry_sem));

	if (ret_val < 0)
		return ret_val;

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
		printf("Debug no lookup value\n");
		result_lookup = -1;
		return result_lookup;
	}

	ret_val = sem_post(&(lookup_table[index].entry_sem));

	if (ret_val < 0)
		return ret_val;

	return 0;
}

/************************************************************************
*
* Function name: lookup_destroy
*        Inputs: None
*        Output: 0 if successful, otherwise -1.
*       Summary: Destroys the lookup count table, and delete inodes if
*                needed.
*
*************************************************************************/

int lookup_destroy()
{
	int count;
	int ret_val;
	LOOKUP_NODE_TYPE *ptr;

	printf("Debug lookup destroy\n");
	for (count = 0; count < NUM_LOOKUP_ENTRIES; count++) {
		ret_val = sem_wait(&(lookup_table[count].entry_sem));

		if (ret_val < 0)
			return ret_val;

		ptr = lookup_table[count].head;

		while (ptr != NULL) {
			printf("Debug check delete %lld\n",
				ptr->this_inode);
			ret_val = disk_checkdelete(ptr->this_inode);

			if (ret_val == 1)
				actual_delete_inode(ptr->this_inode,
						ptr->d_type);
			ptr = ptr->next;
		}

		ret_val = sem_post(&(lookup_table[count].entry_sem));

		if (ret_val < 0)
			return ret_val;
	}

	return 0;
}


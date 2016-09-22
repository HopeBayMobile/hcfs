/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: restoration_utils.h
* Abstract: The c source file for restore operations
*
* Revision History
* 2016/9/22 Kewei created this file.
*
**************************************************************************/

#include "restoration_utils.h"

#include <errno.h>
#include <string.h>

int32_t _inode_bsearch(INODE_PAIR_LIST *list, ino_t src_inode, int32_t *index)
{
	int32_t start_index, end_index, mid_index;
	int64_t cmp_result;

	start_index = 0;
	end_index = list->num_list_entries;
	mid_index = (end_index + start_index) / 2;

	while (end_index > start_index) {

		if (mid_index >= list->list_max_size) {
			mid_index = -1; /* Not found and list is full */
			break;
		}

		if (mid_index >= list->num_list_entries)
			break;

		cmp_result = src_inode - list->inode_pair[mid_index].src_inode;
		if (cmp_result == 0) {
			*index = mid_index;
			return 0;
		} else if (cmp_result < 0) {
			end_index = mid_index;
		} else {
			start_index = mid_index + 1;
		}
		mid_index = (end_index + start_index) / 2;
	}

	/* Key entry not found */
	*index = mid_index;
	return -ENOENT;
}

/**
 * Given pair (src_inode, target_inode), insert them to the sorted list.
 *
 * @param list Structure of inode pair list.
 * @param src_inode Source inode number corresponding to now system
 * @param target_inode Target inode number corresponding to restored system.
 *
 * @return 0 on insertion success, otherwise negation of error code.
 */
int32_t insert_inode_pair(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t target_inode)
{
	int32_t ret;
	int32_t index;

	ret = _inode_bsearch(list, src_inode, &index);
	if (ret == 0)
		return 0;
	if (index < 0)
		return -EFAULT;

	memmove(list->inode_pair + index + 1, list->inode_pair + index,
		(list->num_list_entries - index + 1) * sizeof(INODE_PAIR));
	list->inode_pair[index].src_inode = src_inode;
	list->inode_pair[index].target_inode = target_inode;
	list->num_list_entries += 1;
	if (list->num_list_entries >= list->list_max_size - 1) {
		list->list_max_size += INCREASE_LIST_SIZE;
		list->inode_pair = realloc(list->inode_pair,
			sizeof(INODE_PAIR) * list->list_max_size);
		if (list->inode_pair == NULL)
			return -ENOMEM;
	}

	return 0;
}

/**
 * Given source inode number "src_inode", find the target inode number in list.
 *
 * @param list Structure of inode pair list.
 * @param src_inode Source inode number corresponding to now system
 * @param target_inode Target inode number corresponding to restored system.
 *
 * @return 0 on success, otherwise -ENOENT indicating src_inode not found.
 */
int32_t find_target_inode(INODE_PAIR_LIST *list, ino_t src_inode,
		ino_t *target_inode)
{
	int32_t ret;
	int32_t index;

	ret = _inode_bsearch(list, src_inode, &index);
	if (ret < 0)
		return ret;

	*target_inode = list->inode_pair[index].target_inode;
	return 0;
}

/**
 * Allocate a data structure INODE_PAIR_LIST and return the address.
 *
 * @return Pointer of a INODE_PAIR_LIST list.
 */
INODE_PAIR_LIST *new_inode_pair_list()
{
	INODE_PAIR_LIST *list;

	list = (INODE_PAIR_LIST *)calloc(sizeof(INODE_PAIR_LIST), 1);
	if (!list) {
		write_log(0, "Error: Fail to alloc memory in %s.", __func__);
		return NULL;
	}

	list->list_max_size = INCREASE_LIST_SIZE;
	list->inode_pair = (INODE_PAIR *) calloc(sizeof(INODE_PAIR),
			list->list_max_size);
	if (!(list->inode_pair)) {
		write_log(0, "Error: Fail to alloc memory in %s.", __func__);
		FREE(list);
		return NULL;
	}

	return list;
}

/**
 * Free all memory resource of the list.
 *
 * @return none.
 */
void destroy_inode_pair_list(INODE_PAIR_LIST *list)
{
	FREE(list->inode_pair);
	FREE(list);
}

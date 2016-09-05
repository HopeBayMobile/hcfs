/*************************************************************************
*
* Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: xattr_ops.c
* Abstract: The c source code file for xattr operations. The file includes
*           operation of insert, remove, get, and list xattr corresponding
*           to fuse operaion about xattr.
*
* Revision History
* 2015/6/15 Kewei created the file and add function parse_xattr_namespace().
* 2015/6/16 Kewei added some functions about xattr_insert operation.
* 2015/6/18 Kewei fixed bugs about insert_xattr(). It works now.
* 2015/6/22 Kewei added function get_xattr().
* 2015/6/23 Kewei added function list_xattr().
* 2015/6/24 Kewei added function remove_xattr().
* 2015/8/10 Jiahong revised the file for coding style.
*
**************************************************************************/


#include "xattr_ops.h"

#include <errno.h>
#include <sys/xattr.h>

#include "meta_mem_cache.h"
#include "super_block.h"
#include "string.h"
#include "global.h"
#include "macro.h"

/**
 * Parse input parameter "name"
 *
 * Parse input name into namespace and key, which are separated by '.'.
 * That is, [name] = [name_space].[key]
 *
 * @return 0 if success, otherwise corresponding negavite error code.
 */
int32_t parse_xattr_namespace(const char *name, char *name_space, char *key)
{
	int32_t index;
	int32_t key_len;
	char namespace_string[20];

	/* Find '.' which is used to concatenate namespace and key.
	   eg.: [namespace].[key] */
	index = 0;
	while (name[index]) {
		if (name[index] == '.') /* Find '.' */
			break;
		index++;
	}

	/* No character '.' or namespace not supported, invalid args. */
	if ((name[index] != '.') || (index > 15))
		return -EOPNOTSUPP;

	key_len = strlen(name) - (index + 1);

	/* key len is invalid. */
	if ((key_len >= MAX_KEY_SIZE) || (key_len <= 0))
		return -EINVAL;

	/* Copy namespace */
	memcpy(namespace_string, name, sizeof(char) * index);
	namespace_string[index] = '\0';
	memcpy(key, &name[index+1], sizeof(char) * key_len); /* Copy key */
	key[key_len] = '\0';

	if (!strcmp("user", namespace_string)) {
		*name_space = USER;
		return 0;
	} else if (!strcmp("system", namespace_string)) {
		*name_space = SYSTEM;
		return 0;
	} else if (!strcmp("security", namespace_string)) {
		*name_space = SECURITY;
		return 0;
	} else if (!strcmp("trusted", namespace_string)) {
		*name_space = TRUSTED;
		return 0;
	} else {
		return -EOPNOTSUPP; /* Namespace is not supported. */
	}

	return 0;
}

/**
 * String hash function
 *
 * This hash function is DJB hash with mod.
 *
 * @return hash result
 */
static uint32_t hash(const char *input)
{
	/* FIXME: the string length can be calculated in advance. */
	return djb_hash(input, strlen(input)) % MAX_KEY_HASH_ENTRY;
}

/**
 * Copy data to key_entry
 *
 * Given member data, copy them to corresponding variables in key_entry.
 *
 * @return none.
 */
static inline void copy_key_entry(KEY_ENTRY *key_entry, const char *key,
	int64_t value_pos, size_t size)
{
	strcpy(key_entry->key, key);
	key_entry->key_size = strlen(key);
	key_entry->value_size = size;
	key_entry->first_value_block_pos = value_pos;
}

/**
 * Get a usable file position of key_list_page
 *
 * The function find a appropriate position of KEY_LIST_PAGE. Policy is:
 * Priority 1: Get position from reclaimed key_list_page recorded in XATTR_PAGE
 * Priority 2: Allocate a new position at EOF
 *
 * @return 0 if success, and usable position is stored in "usable_pos".
 */
int32_t get_usable_key_list_filepos(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, int64_t *usable_pos)
{
	int64_t ret_pos;
	KEY_LIST_PAGE key_list_page;
	int32_t errcode;
	int32_t ret;
	int32_t ret_size;
	KEY_LIST_PAGE empty_page;

	/* First get reclaimed page if exist */
	if (xattr_page->reclaimed_key_list_page != 0) {
		ret_pos = xattr_page->reclaimed_key_list_page;

		FSEEK(meta_cache_entry->fptr, ret_pos, SEEK_SET);
		FREAD(&key_list_page, sizeof(KEY_LIST_PAGE), 1,
			meta_cache_entry->fptr);
		/* Reclaimed_list point to next key_list_page */
		xattr_page->reclaimed_key_list_page =
				key_list_page.next_list_pos;
		write_log(5, "Get reclaimed key page, pointing to %lld\n",
			ret_pos);

	} else { /* No reclaimed page, return file pos of EOF */
		FSEEK(meta_cache_entry->fptr, 0, SEEK_END);
		FTELL(meta_cache_entry->fptr); /* Store filepos in ret_pos */

		memset(&empty_page, 0, sizeof(KEY_LIST_PAGE));
		FWRITE(&empty_page, sizeof(KEY_LIST_PAGE), 1,
			meta_cache_entry->fptr);
		write_log(5, "Allocate a new key page, pointing to %lld\n",
			ret_pos);
	}

	*usable_pos = ret_pos;
	return 0;

errcode_handle:
	return errcode;
}

/**
 * Find key entry using binary search.
 *
 * Given a sorted key entry array "key_list", the task is to find the index of
 * string "key" in the array. There are some cases:
 * Case 1: Hit the key entry. Target index is stored in "index".
 * Case 2: Hit nothing and array is not full. "index" stores fitting index to be
 *         inserted.
 * Case 3: Hit nothing but array is full. Then "index" is -1.
 *
 * @return 0 if entry is found. Otherwise return -1.
 */
int32_t key_binary_search(KEY_ENTRY *key_list, uint32_t num_xattr, const char *key,
	int32_t *index)
{
	int32_t start_index, end_index, mid_index;
	int32_t cmp_result;

	start_index = 0;
	end_index = num_xattr;
	mid_index = (end_index + start_index) / 2;

	while (end_index > start_index) {

		if (mid_index >= MAX_KEY_ENTRY_PER_LIST) {
			mid_index = -1; /* Not found and list is full */
			break;
		}

		/* can insert key to last position */
		if ((uint32_t)mid_index >= num_xattr)
			break;

		cmp_result = strcmp(key, key_list[mid_index].key);
		write_log(10, "Debug key_binary_search(): target key = %s, "
			"and in key-array[%d] = %s\n", key, mid_index,
			key_list[mid_index].key);

		if (cmp_result == 0) { /* Hit key entry */
			*index = mid_index;
			return 0;
		}

		if (cmp_result < 0)
			end_index = mid_index;
		else
			start_index = mid_index + 1;
		mid_index = (end_index + start_index) / 2;
	}

	/* Key entry not found */
	*index = mid_index;
	return -1;
}

/**
 * Find key entry in a singly key_list_page linked list.
 *
 * Given first position of key_list_page "first_key_list_pos", the function aims
 * to find out the target key string "key". If it is found, saved the
 * key_list_page
 * position and data, which is stored in "target_key_list_pos" and
 * "target_key_list_page", respectively. If key entry is not found:
 *
 * Case 1: Key entry not found and key entry can be inserted to some
 *         key_list_page.
 *         Then "target_key_list_page" stores the key_list_page that have not
 *         been
 *         full. This key_list_page is the non-full page that first met when
 *         traversing the singly-linked list.
 * Case 2: Key entry not found and unfortunately all key_list_page are full.
 *         Then
 *         the "target_key_list_page" stores the last page in singly-linked
 *         list.
 *
 * The "prev_page" and "prev_pos" are previous key list page & position when
 * key entry is hit, which is used in remove_xattr(). These arguments will be
 * ignored
 * when they are set as NULL. Don't care these two argument when key is not
 * found.
 *
 * @return 0 if entry is found, 1 if entry not found. Return -1 on error.
 */
int32_t find_key_entry(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	int64_t first_key_list_pos, KEY_LIST_PAGE *target_key_list_page,
	int32_t *key_index, int64_t *target_key_list_pos, const char *key,
	KEY_LIST_PAGE *prev_page, int64_t *prev_pos)
{
	int64_t key_list_pos, prev_key_list_pos;
	KEY_LIST_PAGE now_key_page, prev_key_page;
	int32_t ret;
	int32_t index;
	char find_first_insert;
	char hit_key_entry;
	int32_t errcode;
	int32_t ret_size;

	if (first_key_list_pos == 0) { /* Never happen in insert_xattr */
		write_log(10, "Debug xattr: first_key_list_pos is 0\n");
		*key_index = -1;
		return 1;
	}

	memset(&now_key_page, 0, sizeof(KEY_LIST_PAGE));
	memset(&prev_key_page, 0, sizeof(KEY_LIST_PAGE));
	prev_key_list_pos = 0;
	key_list_pos = first_key_list_pos;
	find_first_insert = FALSE;
	hit_key_entry = FALSE;

	do {
		memcpy(&prev_key_page, &now_key_page, sizeof(KEY_LIST_PAGE));

		FSEEK(meta_cache_entry->fptr, key_list_pos, SEEK_SET);
		FREAD(&now_key_page, sizeof(KEY_LIST_PAGE), 1,
			meta_cache_entry->fptr);
		write_log(10, "Debug xattr: now in find_key_entry(), "
			"key_list_pos = %lld, num_xattr = %d\n",
			key_list_pos, now_key_page.num_xattr);

		ret = key_binary_search(now_key_page.key_list,
			now_key_page.num_xattr,
			key, &index);
		if (ret == 0) { /* Hit the key */
			hit_key_entry = TRUE;
			break;
		} /* else record the index that can be inserted. */

		/* Record first page which can be inserted */
		if ((find_first_insert == FALSE) &&
			(now_key_page.num_xattr < MAX_KEY_ENTRY_PER_LIST)) {
			memcpy(target_key_list_page, &now_key_page,
				sizeof(KEY_LIST_PAGE));
			*key_index = index;
			*target_key_list_pos = key_list_pos;
			find_first_insert = TRUE; /* Just need the first one */
		}

		prev_key_list_pos = key_list_pos;
		key_list_pos = now_key_page.next_list_pos; /* Go to next page */

	} while (key_list_pos);

	/* Key existed, return 0 */
	if (hit_key_entry == TRUE) {
		write_log(10, "Debug: key is found.\n");
		memcpy(target_key_list_page, &now_key_page,
			sizeof(KEY_LIST_PAGE));
		*key_index = index;
		*target_key_list_pos = key_list_pos;
		if (prev_page != NULL)
			memcpy(prev_page, &prev_key_page, sizeof(KEY_LIST_PAGE));
		if (prev_pos != NULL)
			*prev_pos = prev_key_list_pos;
		return 0;
	}
	/* Hit nothing, and all key list are full, return the last page! */
	if ((hit_key_entry == FALSE) && (find_first_insert == FALSE)) {
		write_log(10,
			"Debug: Key is NOT found and all key pages are full.\n");
		memcpy(target_key_list_page, &now_key_page,
			sizeof(KEY_LIST_PAGE));
		*key_index = -1;
		*target_key_list_pos = prev_key_list_pos;
		return 1;
	}
	/* Hit nothing, can insert into an entry but don't have to allocate
	   a new page, return the page which can be inserted key. */
	if ((hit_key_entry == FALSE) && (find_first_insert == TRUE)) {
		write_log(10,
			"Debug: Key is NOT found and it can be inserted to "
			"an existed key page.\n");
		return 1;
	}

errcode_handle:
	return errcode;

}

/**
 * Get a usable file position of value_block.
 *
 * When asking a usable position to write value_block. This function first
 * find position in the link-list pointed by "replace_value_block_pos", which is
 * original value blocks pointed by a key entry. This is non-zero in REPLACE
 * situation of insertxattr(). Second return a usable postion from reclaimed
 * value blocks. If there are no reclaimed value blocks, then create a new
 * postion at EOF. A fitting position is stored in "usable_value_pos".
 *
 * @return 0 if success, otherwise negative error code.
 */
int32_t get_usable_value_filepos(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, int64_t *replace_value_block_pos,
	int64_t *usable_value_pos)
{
	VALUE_BLOCK value_block;
	VALUE_BLOCK empty_block;
	int32_t errcode;
	int32_t ret;
	int32_t ret_size;
	int32_t ret_pos;

	/* Priority 1: reuse the replace_value_block_pos when replacing */
	if (*replace_value_block_pos > 0) {
		*usable_value_pos = *replace_value_block_pos;
		/* Point to next replace block */
		FSEEK(meta_cache_entry->fptr, *replace_value_block_pos,
				SEEK_SET);
		FREAD(&value_block, sizeof(VALUE_BLOCK), 1,
				meta_cache_entry->fptr);
		*replace_value_block_pos = value_block.next_block_pos;
		write_log(5, "Get original value block, pointing to %lld\n",
			*usable_value_pos);
		return 0;
	}

	/* Priority 2: return the reclaimed value-block */
	if (xattr_page->reclaimed_value_block > 0) {
		*usable_value_pos = xattr_page->reclaimed_value_block;
		/* Point to next reclaimed block */
		FSEEK(meta_cache_entry->fptr, xattr_page->reclaimed_value_block,
			SEEK_SET);
		FREAD(&value_block, sizeof(VALUE_BLOCK), 1,
			meta_cache_entry->fptr);
		xattr_page->reclaimed_value_block = value_block.next_block_pos;
		write_log(5, "Get reclaimed value block, pointing to %lld\n",
			*usable_value_pos);
		return 0;
	}

	/* Priority 3: Allocate a new value_block at EOF */
	FSEEK(meta_cache_entry->fptr, 0, SEEK_END);
	FTELL(meta_cache_entry->fptr); /* Store filepos in ret_pos */

	memset(&empty_block, 0, sizeof(VALUE_BLOCK));
	FWRITE(&empty_block, sizeof(VALUE_BLOCK), 1, meta_cache_entry->fptr);

	*usable_value_pos = ret_pos;
	write_log(5, "Allocate a new value block, pointing to %lld\n",
		*usable_value_pos);
	return 0;

errcode_handle:
	return errcode;

}

/**
 * Write value data.
 *
 * Write value data such that it in a linked-list. "first_value_pos" is the
 * first
 * value block position to write value data. The function will call
 * get_usable_value_filepos() when need extra value block positions to
 * write value.
 *
 * @return 0 for success, otherwise negative error code.
 */
int32_t write_value_data(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	int64_t *replace_value_block_pos, int64_t first_value_pos,
	const char *value, size_t size)
{
	size_t index;
	VALUE_BLOCK tmp_value_block;
	int64_t now_pos, next_pos = 0;
	int32_t ret_code;
	int32_t errcode;
	int32_t ret, ret_size;


	index = 0;
	now_pos = first_value_pos;
	while (index < size) {
		memset(&tmp_value_block, 0, sizeof(VALUE_BLOCK));
		if (size - index > MAX_VALUE_BLOCK_SIZE) { /* Not last block */
			memcpy(tmp_value_block.content, &value[index],
				sizeof(char) * MAX_VALUE_BLOCK_SIZE);
			ret_code = get_usable_value_filepos(meta_cache_entry,
				xattr_page, replace_value_block_pos, &next_pos);
			if (ret_code < 0) {
				write_log(0, "Error in "
					"get_usable_key_list_filepos()\n");
				return ret_code;
			}

			tmp_value_block.next_block_pos = next_pos;

		} else { /* last value block */
			memcpy(tmp_value_block.content, &value[index],
				sizeof(char) * (size - index));
			tmp_value_block.next_block_pos = 0;
			next_pos = 0;
		}

		FSEEK(meta_cache_entry->fptr, now_pos, SEEK_SET);
		FWRITE(&tmp_value_block, sizeof(VALUE_BLOCK), 1,
			meta_cache_entry->fptr);
		now_pos = next_pos;

		index += MAX_VALUE_BLOCK_SIZE; /* Go to next content */
	}

	return 0;

errcode_handle:
	return errcode;

}

/**
 * Read value data.
 *
 * Read value data in linked-list. Value is stored in "value_buf".
 *
 * @return 0 for success, otherwise negative error code.
 */
int32_t read_value_data(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	KEY_ENTRY *key_entry, char *value_buf)
{
	VALUE_BLOCK tmp_value_block;
	size_t value_size;
	size_t index;
	int64_t now_pos;
	int32_t errcode;
	int32_t ret, ret_size;

	index = 0;
	value_size = key_entry->value_size;
	now_pos = key_entry->first_value_block_pos;

	while (index < value_size) {
		FSEEK(meta_cache_entry->fptr, now_pos, SEEK_SET);
		FREAD(&tmp_value_block, sizeof(VALUE_BLOCK), 1,
			meta_cache_entry->fptr);

		if (value_size - index > MAX_VALUE_BLOCK_SIZE) {
			/* Not last block */
			memcpy(&value_buf[index], tmp_value_block.content,
				sizeof(char) * MAX_VALUE_BLOCK_SIZE);
		} else { /* Last one */
			memcpy(&value_buf[index], tmp_value_block.content,
				sizeof(char) * (value_size - index));
		}

		now_pos = tmp_value_block.next_block_pos;
		index += MAX_VALUE_BLOCK_SIZE; /* Go to next content */
	}

	return 0;

errcode_handle:
	return errcode;

}

/**
 * Recycle a value block linked-list
 *
 * In REPLACE situation of insert_xattr() or remove_xattr(), Perhaps some value
 * blocks need to be reclaimed. The head position of this linked-list is
 * passed in
 * "replace_value_block_pos".
 *
 * @return 0 for success, otherwise negative error code.
 */
int32_t reclaim_replace_value_block(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	XATTR_PAGE *xattr_page, int64_t *replace_value_block_pos)
{

	int64_t head_pos;
	int64_t tail_pos;
	VALUE_BLOCK tmp_value_block;
	int32_t errcode, ret, ret_size;

	if (*replace_value_block_pos == 0) /* Nothing has to be reclaimed */
		return 0;

	head_pos = *replace_value_block_pos;
	tail_pos = head_pos;
	while (TRUE) {
		FSEEK(meta_cache_entry->fptr, tail_pos, SEEK_SET);
		FREAD(&tmp_value_block, sizeof(VALUE_BLOCK), 1,
			meta_cache_entry->fptr);
		if (tmp_value_block.next_block_pos == 0)
			break;
		tail_pos = tmp_value_block.next_block_pos;
	}

	/* Link tail block to first reclaimed block */
	tmp_value_block.next_block_pos = xattr_page->reclaimed_value_block;
	FSEEK(meta_cache_entry->fptr, tail_pos, SEEK_SET);
	FWRITE(&tmp_value_block, sizeof(VALUE_BLOCK), 1,
			meta_cache_entry->fptr);

	/* Link first reclaimed block to head block */
	xattr_page->reclaimed_value_block = head_pos;

	write_log(5,
		"Debug: Now head of reclaimed value blocks points to %lld\n",
		xattr_page->reclaimed_value_block);

	return 0;

errcode_handle:
	return errcode;

}

/*** A debugged function used to print key-value in a key page. ***/
static void print_keys_to_log(KEY_LIST_PAGE *key_page)
{
	uint32_t i;
	for (i = 0 ; i < key_page->num_xattr ; i++) {
		write_log(10,
			"Debug: key[%d] = %s, len = %d, data_pos = %lld\n", i,
			key_page->key_list[i].key,
			key_page->key_list[i].key_size,
			key_page->key_list[i].first_value_block_pos);
	}
}

/**
 * Set a extended attribute
 *
 * The insert_xattr function is parted into 3 steps:
 * Step 1: Find the key and insert into data structure
 * Step 2: Write value data into linked-value_block
 * Step 3: Modify and write xattr header(xattr_page)
 *
 * In Step 1, there are some cases:
 * Case 1: hash_table[index] is never used, so just allocate a new page and
 *         insert
 *         to index 0.
 * Case 2: At least one key_page in hash_table[index]. Find the key entry:
 *       Case 2.1: Entry is found. Replace value or return error
 *       Case 2.2: Entry not found, some key_pages are not full. The new key
 *                 can be
 *                 inserted to existed key_page.
 *       Case 2.3: Entry not found, but all key_page are full. Allocate a new
 *                 page
 *                 and insert to index 0.
 *
 * @return 0 if success to set xattr, otherwise negative error code.
 */
int32_t insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	const int64_t xattr_filepos, const char name_space_c, const char *key,
	const char *value, const size_t size, const int32_t flag)
{
	uint32_t hash_entry;
	NAMESPACE_PAGE *namespace_page;
	KEY_LIST_PAGE target_key_list_page;
	KEY_ENTRY *now_key_entry;
	KEY_ENTRY buf_key_list[MAX_KEY_ENTRY_PER_LIST];
	int32_t ret_code;
	int32_t key_index;
	int64_t first_key_list_pos;
	int64_t target_key_list_pos = 0;
	int64_t value_pos = 0; /* Record position of first value block */
	int32_t errcode;
	int32_t ret;
	int32_t ret_size;
	int32_t name_space = name_space_c;

#ifdef _ANDROID_ENV_
	UNUSED(flag);
#endif
	/* Used to record value block pos when replacing */
	int64_t replace_value_block_pos;

	/* Step1: Find the key entry and appropriate insertion position */
	hash_entry = hash(key); /* Hash the key */
	namespace_page = &(xattr_page->namespace_page[name_space]);

	if (namespace_page->key_hash_table[hash_entry] == 0) {
		/* Allocate if no page */
		if (flag == XATTR_REPLACE) {
			write_log(5, "Error: Replace value but key did not exist\n");
			return -ENODATA;
		}

		/* Get key list pos from "gc list" or "end of file" */
		ret_code = get_usable_key_list_filepos(meta_cache_entry,
			xattr_page, &target_key_list_pos);
		if (ret_code < 0)
			return ret_code;

		/* Assign the position */
		namespace_page->key_hash_table[hash_entry] = target_key_list_pos;
		memset(&target_key_list_page, 0, sizeof(KEY_LIST_PAGE));

		replace_value_block_pos = 0;
		ret_code = get_usable_value_filepos(meta_cache_entry, xattr_page,
			&replace_value_block_pos, &value_pos);
		if (ret_code < 0)
			return ret_code;

		now_key_entry = &(target_key_list_page.key_list[0]);
		copy_key_entry(now_key_entry, key, value_pos, size);

		(target_key_list_page.num_xattr)++; /* # of xattr += 1 */

		FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
		FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,
			meta_cache_entry->fptr);

		/* In namespace, # of xattr += 1 */
		(xattr_page->namespace_page[name_space].num_xattr)++;
		write_log(10, "Debug setxattr: Init a key_list page success\n");

	} else {
		/* At least 1 page exists. Find the entry and "CREATE"
			or "REPLACE" */
		first_key_list_pos = namespace_page->key_hash_table[hash_entry];

		ret_code = find_key_entry(meta_cache_entry, first_key_list_pos,
				&target_key_list_page, &key_index,
				&target_key_list_pos, key, NULL, NULL);

		if (ret_code > 0) { /* Hit nothing, CREATE key and value */

			if (flag == XATTR_REPLACE) {
				write_log(5, "Error: Replace value but key did not exist\n");
				return -ENODATA;
			}

			if (key_index < 0) { /* All key_list are full, allocate new one */
				int64_t usable_pos = 0;

				ret_code = get_usable_key_list_filepos(meta_cache_entry,
						xattr_page, &usable_pos);
				if (ret_code < 0)
					return ret_code;

				/* Link to end of key_list  */
				target_key_list_page.next_list_pos = usable_pos;

				FSEEK(meta_cache_entry->fptr,
					target_key_list_pos, SEEK_SET);
				FWRITE(&target_key_list_page,
					sizeof(KEY_LIST_PAGE),
					1, meta_cache_entry->fptr);

				/* New page at end of the linked-key_list */
				memset(&target_key_list_page, 0,
					sizeof(KEY_LIST_PAGE));
				/* Move to next page pos */
				target_key_list_pos = usable_pos;
				target_key_list_page.next_list_pos = 0;
				key_index = 0;
				write_log(10, "Debug setxattr: Allocate a new key_list_page,"
					" usable_pos = %lld\n", usable_pos);

			} else {
				/* It can be inserted to existed
					target_key_list_page */
				KEY_ENTRY *key_list;
				uint32_t num_remaining;

				key_list = target_key_list_page.key_list;
				num_remaining =
					target_key_list_page.num_xattr
					- key_index;

				/* Shift one entry for elements after
					key_index */
				memcpy(buf_key_list, key_list + key_index,
					sizeof(KEY_ENTRY) * num_remaining);
				memcpy(key_list + (key_index + 1),
					buf_key_list,
					sizeof(KEY_ENTRY) * num_remaining);
				write_log(10, "Debug setxattr: Begin to insert to a"
					" existed key_list_page\n");
			}

			/* Insert key into target_key_list */
			replace_value_block_pos = 0;
			ret_code = get_usable_value_filepos(meta_cache_entry,
				xattr_page,
				&replace_value_block_pos, &value_pos);
			if (ret_code < 0)
				return ret_code;

			now_key_entry =
				&(target_key_list_page.key_list[key_index]);
			copy_key_entry(now_key_entry, key, value_pos, size);
			(target_key_list_page.num_xattr)++;

			FSEEK(meta_cache_entry->fptr, target_key_list_pos,
				SEEK_SET);
			FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,
				meta_cache_entry->fptr);

			/* In namespace, # of xattr += 1 */
			(xattr_page->namespace_page[name_space].num_xattr)++;
			write_log(10, "Debug setxattr: Creating a key success, pos = %lld\n",
				target_key_list_pos);

		} else if (ret_code == 0) {
			/* Hit the key entry, REPLACE value */
			if (flag == XATTR_CREATE) {
				write_log(5, "Error: Create a new key but it existed\n");
				return -EEXIST;
			}

			/* Replace the value size and then rewrite. */
			now_key_entry =
				&(target_key_list_page.key_list[key_index]);
			now_key_entry->value_size = size;

			replace_value_block_pos =
				now_key_entry->first_value_block_pos;
			ret_code = get_usable_value_filepos(meta_cache_entry,
				xattr_page, &replace_value_block_pos,
				&value_pos);
			if (ret_code < 0)
				return ret_code;

			FSEEK(meta_cache_entry->fptr, target_key_list_pos,
				SEEK_SET);
			FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,
				meta_cache_entry->fptr);
			write_log(10, "Debug setxattr: Hit the key. Replacing value success\n");

		} else { /* Error */
			return ret_code;
		}
	}

	/* Step 2: After insert key, then write value data */
	ret_code = write_value_data(meta_cache_entry, xattr_page,
		&replace_value_block_pos, value_pos, value, size);
	if (ret_code < 0)
		return ret_code;

	/* Reclaim remaining value_block (if REPLACE situation) */
	ret_code = reclaim_replace_value_block(meta_cache_entry, xattr_page,
		&replace_value_block_pos);
	if (ret_code < 0)
		return ret_code;

	/* Step 3: Finally write xattr header(xattr_page) */
	FSEEK(meta_cache_entry->fptr, xattr_filepos, SEEK_SET);
	FWRITE(xattr_page, sizeof(XATTR_PAGE), 1, meta_cache_entry->fptr);

	ret_code = super_block_mark_dirty(meta_cache_entry->inode_num);
	if (ret_code < 0)
		return ret_code;

	print_keys_to_log(&target_key_list_page);
	write_log(10, "Debug setxattr: Now number of xattr = %d, "
		"now reclaimed_key_page point to %lld, "
		"and reclaimed_value_block point to %lld\n",
		xattr_page->namespace_page[name_space].num_xattr,
		xattr_page->reclaimed_key_list_page,
		xattr_page->reclaimed_value_block);
	return 0;

errcode_handle:
	return errcode;

}

/**
 * Get extended attribute.
 *
 * This function aims to fill the buffer with value of a given key, and there
 * are four cases:
 * Case 1: Return error if key is not found.
 * Case 2: If buffer size <= 0, assign expected size of value to "actual_size".
 * Case 3: If buffer size is too small, return -ERANGE.
 * Case 4: If buffer is sufficient, fill it with value.
 *
 * @return 0 if success to get xattr or to get needed buffer size,
 *         and error code on error.
 */
int32_t get_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	const char name_space_c, const char *key, char *value_buf,
	const size_t size, size_t *actual_size)
{
	NAMESPACE_PAGE *namespace_page;
	KEY_ENTRY *key_entry;
	KEY_LIST_PAGE target_key_list_page;
	int64_t target_key_list_pos;
	int64_t first_key_list_pos;
	uint32_t hash_index;
	int32_t key_index;
	int32_t ret_code;
	int32_t name_space = name_space_c;

	*actual_size = 0;

	hash_index = hash(key); /* Hash the key */
	namespace_page = &(xattr_page->namespace_page[name_space]);

	if (namespace_page->key_hash_table[hash_index] == 0) {
		write_log(8, "Key %s not found in get_xattr()\n", key);
		return -ENODATA;
	}

	first_key_list_pos = namespace_page->key_hash_table[hash_index];
	ret_code = find_key_entry(meta_cache_entry, first_key_list_pos,
			&target_key_list_page, &key_index,
			&target_key_list_pos, key, NULL, NULL);

	if (ret_code < 0)
		return ret_code;

	if (ret_code > 0) { /* Hit nothing */
		write_log(8, "Key %s not found in get_xattr()\n", key);
		return -ENODATA;
	}

	/* Else, key is found */
	key_entry = &(target_key_list_page.key_list[key_index]);
	*actual_size = key_entry->value_size;
	if (size <= 0) /* Get actual size when size == 0 */
		return 0;

	if (size > 0) {
		if (size < *actual_size) {
			write_log(8, "Error: Size of key buffer is too small\n");
			return -ERANGE;
		}

		ret_code = read_value_data(meta_cache_entry, key_entry,
				value_buf);
		if (ret_code < 0)
			return ret_code;
	}

	return 0; /* Success when size == 0 or finishing to read value */
}

/**
 * Fill buffer with all names
 *
 * Fill the buffer "key_buf" with [name] = [namespace_prefix].[key]. actual_size
 * record total size in the key buffer up to present. "size" is the parameter
 * passed by FUSE listxattr(). If it is zero, just add "size" to "actual_size"
 * rather than fill the key buffer.
 *
 * @return 0 if success, -1 when size is too small.
 */
static int32_t fill_buffer_with_key(const KEY_LIST_PAGE *key_page, char *key_buf,
	const size_t size, size_t *actual_size, const char *namespace_prefix)
{
	uint32_t key_index;
	char tmp_buf[MAX_KEY_SIZE + 50];
	int32_t key_size;
	int32_t namespace_len;

	namespace_len = strlen(namespace_prefix);

	for (key_index = 0; key_index < key_page->num_xattr ; key_index++) {
		key_size = (namespace_len + 1) +
			key_page->key_list[key_index].key_size + 1;

		if (size > 0) {
			if (*actual_size + key_size > size)
				return -1;
			/* combine namespace and key */
			strcpy(tmp_buf, namespace_prefix);
			tmp_buf[namespace_len] = '.';
			strcpy(tmp_buf + namespace_len + 1,
				key_page->key_list[key_index].key);
			tmp_buf[key_size - 1] = '\0';

			memcpy(&key_buf[*actual_size], tmp_buf,
				sizeof(char) * key_size);
		}

		*actual_size += key_size;
	}

	return 0;
}

/**
 * List all xattr names stored in meta.
 *
 * This function mainly aims to fill the buffer with all names in meta, which is
 * separated by null characters. If the parameter "size" is zero, then this
 * function will find out the needed size of the buffer such that FUSE will
 * allocate a appropriate name buffer with sufficient size, and then fill the
 * buffer with all names next time.
 *
 * @return 0 if success to fill the name buffer or to find out needed size,
 *         otherwise return negative error code.
 */
int32_t list_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	char *key_buf, const size_t size, size_t *actual_size)
{
	NAMESPACE_PAGE *namespace_page;
	KEY_LIST_PAGE key_page;
	int32_t ns_count;
	int32_t hash_count;
	int32_t ret, ret_size, errcode;
	char namespace_prefix[30];

	*actual_size = 0;

	for (ns_count = 0; ns_count < 4; ns_count++) {
		namespace_page = &(xattr_page->namespace_page[ns_count]);
		if (namespace_page->num_xattr == 0)
			continue;

		memset(namespace_prefix, 0, sizeof(char) * 30);
		switch (ns_count) {
		case USER:
			strcpy(namespace_prefix, "user");
			break;
		case SYSTEM:
			strcpy(namespace_prefix, "system");
			break;
		case SECURITY:
			strcpy(namespace_prefix, "security");
			break;
		case TRUSTED:
			strcpy(namespace_prefix, "trusted");
			break;
		default:
			return -EOPNOTSUPP;
		}

		for (hash_count = 0; hash_count < MAX_KEY_HASH_ENTRY;
				hash_count++) {
			int64_t pos, first_pos;

			first_pos = namespace_page->key_hash_table[hash_count];
			pos = first_pos;
			while (pos) {
				FSEEK(meta_cache_entry->fptr, pos, SEEK_SET);
				FREAD(&key_page, sizeof(KEY_LIST_PAGE), 1,
					meta_cache_entry->fptr);
				ret = fill_buffer_with_key(&key_page, key_buf,
					size, actual_size, namespace_prefix);
				if (ret < 0) {
					write_log(8, "Error: Size of buffer is "
						"too small in list_xattr()\n");
					return -ERANGE;
				}
				pos = key_page.next_list_pos;
			}
		}
	}

	return 0;

errcode_handle:
	return errcode;
}

/**
 * Remove the xattr.
 *
 * Remove xattr if key is found, and some corresponding garbage collection
 * is processed
 * when it is needed. The function first deletes the key entry and then reclaims
 * the value blocks. All fwrite operations in this function is safe
 * when crashing.
 *
 * @return 0 if key exists and success to delete, otherwise return error code.
 */
int32_t remove_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
		XATTR_PAGE *xattr_page,
	const int64_t xattr_filepos, const char name_space_c, const char *key)
{
	NAMESPACE_PAGE *namespace_page;
	KEY_LIST_PAGE target_key_list_page;
	KEY_LIST_PAGE prev_key_list_page;
	KEY_ENTRY tmp_key_buf[MAX_KEY_SIZE];
	int64_t target_key_list_pos = 0;
	int64_t first_key_list_pos;
	int64_t prev_key_list_pos = 0;
	int64_t first_value_pos;
	int32_t key_index = 0;
	int32_t hash_index;
	int32_t ret_code;
	int32_t num_remaining;
	int32_t errcode, ret, ret_size;
	int32_t name_space = name_space_c;

	memset(&target_key_list_page, 0, sizeof(KEY_LIST_PAGE));

	hash_index = hash(key); /* Hash the key */
	namespace_page = &(xattr_page->namespace_page[name_space]);

	first_key_list_pos = namespace_page->key_hash_table[hash_index];
	ret_code = find_key_entry(meta_cache_entry, first_key_list_pos,
			&target_key_list_page, &key_index,
			&target_key_list_pos, key, &prev_key_list_page,
			&prev_key_list_pos);
	if (ret_code < 0) /* Error */
		return ret_code;
	if (ret_code > 0) { /* Hit nothing */
		write_log(10, "Debug removexattr:Key %s does not exist\n", key);
		return -ENODATA;
	}

	/* Record value block position, and reclaim them later. */
	first_value_pos =
		target_key_list_page.key_list[key_index].first_value_block_pos;

	/* Don't need to be reclaimed. Just remove the key entry */
	if (target_key_list_page.num_xattr > 1) {
		num_remaining = target_key_list_page.num_xattr - key_index - 1;
		memcpy(tmp_key_buf,
			&(target_key_list_page.key_list[key_index + 1]),
			sizeof(KEY_ENTRY) * num_remaining);
		memcpy(&(target_key_list_page.key_list[key_index]), tmp_key_buf,
			sizeof(KEY_ENTRY) * num_remaining);
		(target_key_list_page.num_xattr)--;

		FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
		FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,
			meta_cache_entry->fptr);

		print_keys_to_log(&target_key_list_page);

	} else { /* Reclaim the key_list_page */
		int64_t reclaim_key_list_pos;
		KEY_LIST_PAGE tmp_list_page;

		/* Case 1: target_page == first_page */
		if (target_key_list_pos == first_key_list_pos) {

			namespace_page->key_hash_table[hash_index] =
				target_key_list_page.next_list_pos;
			FSEEK(meta_cache_entry->fptr, xattr_filepos,
					SEEK_SET);
			FWRITE(xattr_page, sizeof(XATTR_PAGE),
					1, meta_cache_entry->fptr);

			/* Reclaimed page is current page. */
			reclaim_key_list_pos = target_key_list_pos;

		} else { /* Case 2: target_page is internal page */

			prev_key_list_page.next_list_pos =
				target_key_list_page.next_list_pos;

			FSEEK(meta_cache_entry->fptr, prev_key_list_pos,
				SEEK_SET);
			FWRITE(&prev_key_list_page, sizeof(KEY_LIST_PAGE),
				1, meta_cache_entry->fptr);

			/* Reclaim current page */
			reclaim_key_list_pos = target_key_list_pos;
		}

		/* Reclaim key page */
		memset(&tmp_list_page, 0, sizeof(KEY_LIST_PAGE));
		tmp_list_page.next_list_pos =
			xattr_page->reclaimed_key_list_page;

		FSEEK(meta_cache_entry->fptr, reclaim_key_list_pos, SEEK_SET);
		FWRITE(&tmp_list_page, sizeof(KEY_LIST_PAGE), 1,
			meta_cache_entry->fptr);

		xattr_page->reclaimed_key_list_page = reclaim_key_list_pos;
		FSEEK(meta_cache_entry->fptr, xattr_filepos, SEEK_SET);
		FWRITE(xattr_page, sizeof(XATTR_PAGE), 1,
			meta_cache_entry->fptr);
	}

	/* Recalim value block */
	ret_code = reclaim_replace_value_block(meta_cache_entry, xattr_page,
		&first_value_pos);
	if (ret_code < 0)
		return ret_code;

	(xattr_page->namespace_page[name_space].num_xattr)--;
	FSEEK(meta_cache_entry->fptr, xattr_filepos, SEEK_SET);
	FWRITE(xattr_page, sizeof(XATTR_PAGE), 1, meta_cache_entry->fptr);

	ret_code = super_block_mark_dirty(meta_cache_entry->inode_num);
	if (ret_code < 0)
		return ret_code;

	write_log(10, "Debug removexattr: Now number of xattr = %d, "
		"now reclaimed_key_page point to %lld, "
		"and reclaimed_value_block point to %lld\n",
		xattr_page->namespace_page[name_space].num_xattr,
		xattr_page->reclaimed_key_list_page,
		xattr_page->reclaimed_value_block);

	return 0;

errcode_handle:
	return errcode;
}



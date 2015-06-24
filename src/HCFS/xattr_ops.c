/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
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
*
**************************************************************************/


#include <errno.h>
#include "xattr_ops.h"
#include "meta_mem_cache.h"
#include "string.h"
#include "global.h"
#include "macro.h"

int parse_xattr_namespace(const char *name, char *name_space, char *key)
{
	int index;
	int key_len;
	char namespace_string[20];

	/* Find '.' which is used to combine namespace and key.
	   eg.: [namespace].[key] */
	index = 0;
	while (name[index]) {
		if (name[index] == '.') /* Find '.' */
			break;
		index++;
	}
	
	if (name[index] != '.') /* No character '.', invalid args. */
		return -EOPNOTSUPP;
	
	key_len = strlen(name) - (index + 1);
	if ((key_len >= MAX_KEY_SIZE) || (key_len <= 0)) /* key len is invalid. */
		return -EINVAL;

	memcpy(namespace_string, name, sizeof(char) * index); /* Copy namespace */
	namespace_string[index] = '\0';
	memcpy(key, &name[index+1], sizeof(char) * key_len); /* Copy key */
	key[key_len] = '\0';
		
	if (!strcmp("user", namespace_string)) {
		*name_space = USER;
		return 0;
	} else if(!strcmp("system", namespace_string)) {
		*name_space = SYSTEM;
		return 0;
	} else if(!strcmp("security", namespace_string)) {
		*name_space = SECURITY;
		return 0;
	} else if(!strcmp("trusted", namespace_string)) {
		*name_space = TRUSTED;
		return 0;
	} else {	
		return -EOPNOTSUPP; /* Namespace is not supported. */
	}

	return 0;
}

/* Hash function */
static unsigned hash(const char *input) 
{
	unsigned hash = 5381;
	int index;

	index = 0;
	while (input[index]) {
		hash = (((hash << 5) + hash + input[index]) % MAX_KEY_HASH_ENTRY);
		index++;
	}
	
	return hash;
}

static void copy_key_entry(KEY_ENTRY *key_entry, const char *key, 
	long long value_pos, size_t size)
{
	strcpy(key_entry->key, key);
	key_entry->key_size = strlen(key);
	key_entry->value_size = size;
	key_entry->first_value_block_pos = value_pos;
}

int get_usable_key_list_filepos(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, long long *usable_pos)
{
	long long ret_pos;
	KEY_LIST_PAGE key_list_page;
	int errcode; 
	int ret;
	int ret_size;
	KEY_LIST_PAGE empty_page;

	if (xattr_page->reclaimed_key_list_page != 0) { /* Get reclaimed page if exist */
		ret_pos = xattr_page->reclaimed_key_list_page;

		FSEEK(meta_cache_entry->fptr, ret_pos, SEEK_SET);
		FREAD(&key_list_page, sizeof(KEY_LIST_PAGE), 1, 
			meta_cache_entry->fptr);
		/* Reclaimed_list point to next key_list_page */
		xattr_page->reclaimed_key_list_page = key_list_page.next_list_pos;

	} else { /* No reclaimed page, return file pos of EOF */
		FSEEK(meta_cache_entry->fptr, 0, SEEK_END);
		FTELL(meta_cache_entry->fptr); /* Store filepos in ret_pos */

		memset(&empty_page, 0, sizeof(KEY_LIST_PAGE));
		FWRITE(&empty_page, sizeof(KEY_LIST_PAGE), 1, meta_cache_entry->fptr);
	}

	*usable_pos = ret_pos;
	return 0;

errcode_handle:
	return errcode;
}


int key_binary_search(KEY_ENTRY *key_list, unsigned num_xattr, const char *key, 
	int *index)
{
	int start_index, end_index, mid_index;
	int cmp_result;
	
	start_index = 0;
	end_index = num_xattr;
	while (end_index >= start_index) {
		mid_index = (end_index + start_index) / 2;

		if (mid_index >= MAX_KEY_ENTRY_PER_LIST) {
			mid_index = -1; /* Not found and list is full */
			break;
		}
		if (mid_index >= num_xattr) /* can insert key to last position */
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
			end_index = mid_index - 1;
		else
			start_index = mid_index + 1;
	}
		
	/* Key entry not found */
	*index = mid_index;
	return -1;
}

int find_key_entry(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	long long first_key_list_pos, KEY_LIST_PAGE *target_key_list_page, 
	int *key_index, long long *target_key_list_pos, const char *key)
{	
	long long key_list_pos, prev_key_list_pos;
	KEY_LIST_PAGE now_page;
	int ret;
	int index;
	char find_first_insert;
	char hit_key_entry;
	int errcode;
	int ret_size;

	if (first_key_list_pos == 0) { /* Never happen in insert_xattr */
		write_log(10, "Debug xattr: first_key_list_pos is 0\n");
		*key_index = -1;
		return 1;
	}

	memset(&now_page, 0, sizeof(KEY_LIST_PAGE));
	key_list_pos = first_key_list_pos;
	find_first_insert = FALSE; 
	hit_key_entry = FALSE;

	while (key_list_pos) {
		FSEEK(meta_cache_entry->fptr, key_list_pos, SEEK_SET);
		FREAD(&now_page, sizeof(KEY_LIST_PAGE), 1, meta_cache_entry->fptr);
		
		write_log(10, "Debug xattr: now in find_key_entry(), "
			"key_list_pos = %lld, num_xattr = %d\n", 
			key_list_pos, now_page.num_xattr);
		
		ret = key_binary_search(now_page.key_list, now_page.num_xattr, key, &index);
		
		if (ret == 0) { /* Hit the key */
			hit_key_entry = TRUE;
			break;
		} /* else record the index that can be inserted. */
		
		/* Record first page which can be inserted */
		if ((find_first_insert == FALSE) && 
			(now_page.num_xattr < MAX_KEY_ENTRY_PER_LIST)) { 
			memcpy(target_key_list_page, &now_page, sizeof(KEY_LIST_PAGE));
			*key_index = index;
			*target_key_list_pos = key_list_pos;
			find_first_insert = TRUE; /* Just need the first one */
		}
		
		prev_key_list_pos = key_list_pos;
		key_list_pos = now_page.next_list_pos; /* Go to next page */
	}
	
	/* Key has been exist, return 0 */
	if (hit_key_entry == TRUE) { 
		memcpy(target_key_list_page, &now_page, sizeof(KEY_LIST_PAGE));
		*key_index = index;
		*target_key_list_pos = key_list_pos;
		return 0;
	}
	/* All key list are full, return the last page! */
	if ((hit_key_entry == FALSE) && (find_first_insert == FALSE)) {
		memcpy(target_key_list_page, &now_page, sizeof(KEY_LIST_PAGE));
		*key_index = -1;
		*target_key_list_pos = prev_key_list_pos;
		return 1;
	}
	/* Hit nothing, can insert into an entry but don't have to allocate
	   a new page, return the page which can be inserted key. */
	if ((hit_key_entry == FALSE) && (find_first_insert == TRUE)) {
		return 1;
	}

errcode_handle:
	return errcode;

}

int get_usable_value_filepos(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, long long *replace_value_block_pos, 
	long long *usable_value_pos)
{
	VALUE_BLOCK value_block;
	VALUE_BLOCK empty_block;
	int errcode;
	int ret;
	int ret_size;
	int ret_pos;

	/* Priority 1: reuse the replace_value_block_pos when replacing */
	if (*replace_value_block_pos > 0) {	
		*usable_value_pos = *replace_value_block_pos;
		/* Point to next replace block */
		FSEEK(meta_cache_entry->fptr, *replace_value_block_pos, SEEK_SET);
		FREAD(&value_block, sizeof(VALUE_BLOCK), 1, meta_cache_entry->fptr);
		*replace_value_block_pos = value_block.next_block_pos;
		return 0;
	}

	/* Priority 2: return the reclaimed value-block */
	if (xattr_page->reclaimed_value_block > 0) {
		*usable_value_pos = xattr_page->reclaimed_value_block;
		/* Point to next reclaimed block */
		FSEEK(meta_cache_entry->fptr, xattr_page->reclaimed_value_block, 
			SEEK_SET);
		FREAD(&value_block, sizeof(VALUE_BLOCK), 1, meta_cache_entry->fptr);
		xattr_page->reclaimed_value_block = value_block.next_block_pos;
		return 0;
	}

	/* Priority 3: Allocate a new value_block at EOF */
	FSEEK(meta_cache_entry->fptr, 0, SEEK_END);
	FTELL(meta_cache_entry->fptr); /* Store filepos in ret_pos */
	
	memset(&empty_block, 0, sizeof(VALUE_BLOCK));
	FWRITE(&empty_block, sizeof(VALUE_BLOCK), 1, meta_cache_entry->fptr);
	
	*usable_value_pos = ret_pos;
	return 0;

errcode_handle:
	return errcode;

}

int write_value_data(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page,
	long long *replace_value_block_pos, long long first_value_pos, 
	const char *value, size_t size)
{
	size_t index;
	VALUE_BLOCK tmp_value_block;
	long long now_pos, next_pos;
	int ret_code;
	int errcode;
	int ret, ret_size;


	index = 0;
	now_pos = first_value_pos;
	while (index < size) {
		memset(&tmp_value_block, 0, sizeof(VALUE_BLOCK));	
		if (size - index > MAX_VALUE_BLOCK_SIZE) { /* Not last block */
			memcpy(tmp_value_block.content, &value[index], 
				sizeof(char) * MAX_VALUE_BLOCK_SIZE);	
			ret_code = get_usable_value_filepos(meta_cache_entry, 
				xattr_page, replace_value_block_pos, &next_pos);
			if (ret_code < 0)
				return ret_code;

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


int read_value_data(META_CACHE_ENTRY_STRUCT *meta_cache_entry, KEY_ENTRY *key_entry, 
	char *value_buf)
{
	VALUE_BLOCK tmp_value_block;
	size_t value_size;
	size_t index;
	long long now_pos;
	int errcode;
	int ret, ret_size;

	index = 0;
	value_size = key_entry->value_size;
	now_pos = key_entry->first_value_block_pos;

	while (index < value_size) {
		FSEEK(meta_cache_entry->fptr, now_pos, SEEK_SET);
		FREAD(&tmp_value_block, sizeof(VALUE_BLOCK), 1, 
			meta_cache_entry->fptr);
		
		if (value_size - index > MAX_VALUE_BLOCK_SIZE) { /* Not last block */
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


int reclaim_replace_value_block(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	XATTR_PAGE *xattr_page, long long *replace_value_block_pos)
{
	
	long long head_pos;
	long long tail_pos;
	VALUE_BLOCK tmp_value_block;
	int errcode, ret, ret_size;

	if (*replace_value_block_pos == 0) /* Nothing has to be reclaimed */
		return 0;

	head_pos = *replace_value_block_pos;
	tail_pos = head_pos;
	while(TRUE) {
		FSEEK(meta_cache_entry->fptr, tail_pos, SEEK_SET);
		FREAD(&tmp_value_block, sizeof(VALUE_BLOCK), 1, meta_cache_entry->fptr);
		if (tmp_value_block.next_block_pos == 0)
			break;
		tail_pos = tmp_value_block.next_block_pos;
	}
	
	/* Link tail block to first reclaimed block */	
	tmp_value_block.next_block_pos = xattr_page->reclaimed_value_block;
	FSEEK(meta_cache_entry->fptr, tail_pos, SEEK_SET);
	FWRITE(&tmp_value_block, sizeof(VALUE_BLOCK), 1, meta_cache_entry->fptr);
	
	/* Link first reclaimed block to head block */
	xattr_page->reclaimed_value_block = head_pos;

	return 0;

errcode_handle:
	return errcode;

}

/* A debugged function used to print key-value. It will be removed later. */
static void print_key(KEY_LIST_PAGE *key_page, META_CACHE_ENTRY_STRUCT *body)
{
	int i;
	for (i = 0 ; i < key_page->num_xattr ; i++) {
		long long pos = key_page->key_list[i].first_value_block_pos;
		VALUE_BLOCK block;

		fseek(body->fptr, pos, SEEK_SET);
		fread(&block, sizeof(VALUE_BLOCK), 1, body->fptr);

		write_log(10, "key[%d] = %s, len = %d, data = %s, data_pos = %lld\n", i, 
			key_page->key_list[i].key, 
			key_page->key_list[i].key_size,
			block.content,
			key_page->key_list[i].first_value_block_pos);
	}
}

/*
	The insert_xattr function is parted into 3 steps:
	Step 1: Find the key and insert into data structure
	Step 2: Write value data into linked-value_block
	Step 3: Modify and write xattr header(xattr_page)
 */
int insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	const long long xattr_filepos, const char name_space, const char *key, 
	const char *value, const size_t size)
{
	unsigned hash_entry;
	NAMESPACE_PAGE *namespace_page;
	KEY_LIST_PAGE target_key_list_page;
	KEY_ENTRY *now_key_entry;
	KEY_ENTRY buf_key_list[MAX_KEY_ENTRY_PER_LIST];
	int ret_code;
	int key_index;
	long long first_key_list_pos;
	long long target_key_list_pos;
	long long value_pos; /* Record position of first value block */
	int errcode;
	int ret;
	int ret_size;
	long long replace_value_block_pos; /* Used to record value block pos when replacing */

	/* Step1: Find the key entry and appropriate insertion position */
	hash_entry = hash(key); /* Hash the key */
	namespace_page = &(xattr_page->namespace_page[name_space]);
		
	if (namespace_page->key_hash_table[hash_entry] == 0) { /* Allocate if no page */
		/* Get key list pos from "gc list" or "end of file" */
		ret_code = get_usable_key_list_filepos(meta_cache_entry, 
			xattr_page, &target_key_list_pos);
		if (ret_code < 0)
			return ret_code;

		/* Assign the position */
		namespace_page->key_hash_table[hash_entry] = target_key_list_pos; 
		memset(&target_key_list_page, 0, sizeof(KEY_LIST_PAGE)); /* Init page */
		replace_value_block_pos = 0;
		ret_code = get_usable_value_filepos(meta_cache_entry, xattr_page, 
			&replace_value_block_pos, &value_pos); /* Get first value block pos */
		if (ret_code < 0)
			return ret_code;
		
		now_key_entry = &(target_key_list_page.key_list[0]); /* index = 0 */
		copy_key_entry(now_key_entry, key, value_pos, size); /* Copy entry */

		(target_key_list_page.num_xattr)++; /* # of xattr += 1 */

		FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
		FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1, 
			meta_cache_entry->fptr);
		
		/* In namespace, # of xattr += 1 */
		(xattr_page->namespace_page[name_space].num_xattr)++;
		write_log(10, "Debug setxattr: Init a key_list page success\n");

	} else { /* At least 1 page exists. Find the entry and "CREATE" or "REPLACE" */
		first_key_list_pos = namespace_page->key_hash_table[hash_entry];

		ret_code = find_key_entry(meta_cache_entry, first_key_list_pos, 
				&target_key_list_page, &key_index, 
				&target_key_list_pos, key);

		if (ret_code > 0) { /* Hit nothing, CREATE key and value */

			if (key_index < 0) { /* All key_list are full, allocate new one */
				long long usable_pos;

				ret_code = get_usable_key_list_filepos(meta_cache_entry, 
						xattr_page, &usable_pos);
				if (ret_code < 0)
					return ret_code;
				/* Link to end of key_list  */
				target_key_list_page.next_list_pos = usable_pos; 

				FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
				FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 
						1, meta_cache_entry->fptr);
				
				/* New page at end of the linked-key_list */
				memset(&target_key_list_page, 0, sizeof(KEY_LIST_PAGE));
				target_key_list_pos = usable_pos; /* Move to next page pos */
				key_index = 0;
				write_log(10, "Debug setxattr: Allocate a new key_list_page,"
					" usable_pos = %lld\n", usable_pos);

			} else { /* Hit nothing, but can insert to target_key_list_page */
				KEY_ENTRY *key_list;
				unsigned num_remaining;
				
				key_list = target_key_list_page.key_list;
				num_remaining = target_key_list_page.num_xattr - key_index;
				
				/* Shift one entry for elements after key_index */
				memcpy(buf_key_list, key_list + key_index,
					sizeof(KEY_ENTRY) * num_remaining);
				memcpy(key_list + (key_index + 1), 
					buf_key_list, sizeof(KEY_ENTRY) * num_remaining);
				write_log(10, "Debug setxattr: Begin to insert to a" 
					" existed key_list_page\n");
			}

			/* Insert key into target_key_list */
			replace_value_block_pos = 0;
			ret_code = get_usable_value_filepos(meta_cache_entry, xattr_page, 
				&replace_value_block_pos, &value_pos); /* Get first value block pos */
			if (ret_code < 0)
				return ret_code;
			
			now_key_entry = &(target_key_list_page.key_list[key_index]);
			copy_key_entry(now_key_entry, key, value_pos, size); /* Copy entry */
			(target_key_list_page.num_xattr)++; /* # of xattr += 1 */
			
			FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
			FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,  /* Write data */
				meta_cache_entry->fptr);
			
			/* In namespace, # of xattr += 1 */
			(xattr_page->namespace_page[name_space].num_xattr)++;
			write_log(10, "Debug setxattr: Creating a key success, pos = %lld\n", 
				target_key_list_pos);

		} else if (ret_code == 0) { /* Hit the key entry, REPLACE value */
			/* Replace the value size and then rewrite. */
			now_key_entry = &(target_key_list_page.key_list[key_index]);
			now_key_entry->value_size = size; 

			replace_value_block_pos = now_key_entry->first_value_block_pos;
			ret_code = get_usable_value_filepos(meta_cache_entry, xattr_page, 
				&replace_value_block_pos, &value_pos); /* Get first value block pos */
			if (ret_code < 0)
				return ret_code;
			
			FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
			FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,  /* Write data */
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
		
	print_key(&target_key_list_page, meta_cache_entry);
	
	/* Step 3: Finally write xattr header(xattr_page) */
	FSEEK(meta_cache_entry->fptr, xattr_filepos, SEEK_SET);
	FWRITE(xattr_page, sizeof(XATTR_PAGE), 1, meta_cache_entry->fptr);
	write_log(10, "Debug setxattr: Now number of xattr = %d, and "
		"reclaimed_value_block point to %lld\n", 
		xattr_page->namespace_page[name_space].num_xattr, 
		xattr_page->reclaimed_value_block);
	return 0;

errcode_handle:
	return errcode;

}

/*
	This function aims to fill the buffer with value of a given key, and there
	are four cases:
	1. Return error if key is not found.
	2. If buffer size <= 0, assign expected size of value to "actual_size".
	3. If buffer size is too small, return -ERANGE.
	4. If buffer is sufficient, fill it with value.
*/

int get_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	const long long xattr_filepos, const char name_space, const char *key, 
	char *value_buf, const size_t size, size_t *actual_size)
{
	NAMESPACE_PAGE *namespace_page;
	KEY_ENTRY *key_entry;
	KEY_LIST_PAGE target_key_list_page;
	long long target_key_list_pos;
	long long first_key_list_pos;
	unsigned hash_index;
	unsigned key_index;
	int ret_code;

	hash_index = hash(key); /* Hash the key */
	namespace_page = &(xattr_page->namespace_page[name_space]);

	if (namespace_page->key_hash_table[hash_index] == 0)
		return -ENOENT;
	
	first_key_list_pos = namespace_page->key_hash_table[hash_index];
	ret_code = find_key_entry(meta_cache_entry, first_key_list_pos, 
			&target_key_list_page, &key_index, 
			&target_key_list_pos, key);
	
	if (ret_code < 0)
		return ret_code;
	
	if (ret_code > 0) /* Hit nothing */
		return -ENOENT;
	
	/* Else, key is found */
	key_entry = &(target_key_list_page.key_list[key_index]);
	*actual_size = key_entry->value_size; 
	if (size <= 0) /* Get actual size when size == 0 */
		return 0;

	if (size > 0) {
		if (size < *actual_size)
			return -ERANGE;
		
		ret_code = read_value_data(meta_cache_entry, key_entry, value_buf);
		if (ret_code < 0)
			return ret_code;
	}

	return 0; /* Success when size == 0 or finishing to read value */
}

static int fill_buffer_with_key(const KEY_LIST_PAGE *key_page, char *key_buf, 
	const size_t size, size_t *actual_size, const char *namespace_prefix)
{	
	int key_index;
	char tmp_buf[MAX_KEY_SIZE + 50];
	int key_size;
	int namespace_len;

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
			
			write_log(10, "key = %s\n", tmp_buf);
		}
		
		*actual_size += key_size;
	}

	return 0;
}

int list_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	const long long xattr_filepos, char *key_buf, const size_t size, 
	size_t *actual_size)
{
	NAMESPACE_PAGE *namespace_page;
	KEY_LIST_PAGE key_page;
	int ns_count;
	int hash_count;
	int ret, ret_size, errcode;
	char namespace_prefix[30];

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
			break;
		}

		for (hash_count = 0; hash_count < MAX_KEY_HASH_ENTRY ; hash_count++) {
			long long pos;
			
			pos = namespace_page->key_hash_table[hash_count];
			while (pos) {
				FSEEK(meta_cache_entry->fptr, pos, SEEK_SET);
				FREAD(&key_page, sizeof(KEY_LIST_PAGE), 1, 
					meta_cache_entry->fptr);
				ret = fill_buffer_with_key(&key_page, key_buf, size, 
					actual_size, namespace_prefix);
				if (ret < 0)
					return -ERANGE;
				pos = key_page.next_list_pos;
			}
		}
	}

	return 0;

errcode_handle:
	return errcode;
}


static int find_prev_key_page(META_CACHE_ENTRY_STRUCT *meta_cache_entry,
	const long long first_key_list_pos, KEY_LIST_PAGE *prev_key_list_page, 
	long long *prev_key_list_pos, const long long target_key_list_pos)
{
	long long now_pos;
	int errcode;
	int ret, ret_size;

	if (first_key_list_pos == target_key_list_pos) {
		*prev_key_list_pos = 0;
		return 0;
	}

	now_pos = first_key_list_pos;
	while (now_pos) {
		FSEEK(meta_cache_entry->fptr, now_pos, SEEK_SET);
		FREAD(prev_key_list_page, sizeof(KEY_LIST_PAGE), 1, 
			meta_cache_entry->fptr);
		
		if (prev_key_list_page->next_list_pos == target_key_list_pos) {
			*prev_key_list_pos = now_pos;
			return 0;
		}

		now_pos = prev_key_list_page->next_list_pos;
	}

	return -EFAULT;

errcode_handle:
	return errcode;

}

int remove_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	const long long xattr_filepos, const char name_space, const char *key)
{
	NAMESPACE_PAGE *namespace_page;
	KEY_LIST_PAGE target_key_list_page;
	KEY_ENTRY tmp_key_buf[MAX_KEY_SIZE];
	long long target_key_list_pos;
	long long first_key_list_pos;
	long long first_value_pos;
	int key_index;
	int hash_index;
	int ret_code;
	int num_remaining;
	int errcode, ret, ret_size;
		
	hash_index = hash(key); /* Hash the key */
	namespace_page = &(xattr_page->namespace_page[name_space]);

	first_key_list_pos = namespace_page->key_hash_table[hash_index];
	ret_code = find_key_entry(meta_cache_entry, first_key_list_pos, 
			&target_key_list_page, &key_index, 
			&target_key_list_pos, key);	
	if (ret_code < 0) /* Error */
		return ret_code;
	if (ret_code > 0) /* Hit nothing */
		return -ENOENT;
	
	/* Record value block position, and reclaim them later. */
	first_value_pos = target_key_list_page.key_list[key_index].first_value_block_pos;
	
	if (target_key_list_page.num_xattr > 1) { /* Don't need to be reclaimed. */
		/* Just remove the key entry */
		num_remaining = target_key_list_page.num_xattr - key_index - 1; 
		memcpy(tmp_key_buf, &(target_key_list_page.key_list[key_index + 1]),
			sizeof(KEY_ENTRY) * num_remaining);
		memcpy(&(target_key_list_page.key_list[key_index]), tmp_key_buf, 
			sizeof(KEY_ENTRY) * num_remaining);
		(target_key_list_page.num_xattr)--;
		
		FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
		FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1, 
			meta_cache_entry->fptr);

	} else { /* Reclaim the key_list_page */
		long long reclaim_key_list_pos;
		KEY_LIST_PAGE tmp_list_page;

		if (target_key_list_page.next_list_pos == 0) { /* Last page */
			KEY_LIST_PAGE prev_key_list_page;
			long long prev_key_list_pos;

			ret_code = find_prev_key_page(meta_cache_entry, first_key_list_pos, 
					&prev_key_list_page, &prev_key_list_pos,
					target_key_list_pos);
			if (ret_code < 0)
				return ret_code;
			
			if (prev_key_list_pos == 0) { /* Previous page does not exist */
				namespace_page->key_hash_table[hash_index] = 0;
				FSEEK(meta_cache_entry->fptr, xattr_filepos, 
					SEEK_SET);
				FWRITE(xattr_page, sizeof(XATTR_PAGE), 
						1, meta_cache_entry->fptr);
			} else { /* Prev page is found */
				prev_key_list_page.next_list_pos = 0;
				FSEEK(meta_cache_entry->fptr, prev_key_list_pos, 
					SEEK_SET);
				FWRITE(&prev_key_list_page, sizeof(KEY_LIST_PAGE), 
					1, meta_cache_entry->fptr);
			}
			
			reclaim_key_list_pos = target_key_list_pos;

		} else {
			KEY_LIST_PAGE next_key_list_page;
			
			reclaim_key_list_pos = target_key_list_page.next_list_pos;
			
			/* Read next page */
			FSEEK(meta_cache_entry->fptr, 
				target_key_list_page.next_list_pos, SEEK_SET);
			FREAD(&next_key_list_page, sizeof(KEY_LIST_PAGE), 1, 
				meta_cache_entry->fptr);
			/* Write to now position */
			FSEEK(meta_cache_entry->fptr, target_key_list_pos, 
				SEEK_SET);
			FWRITE(&next_key_list_page, sizeof(KEY_LIST_PAGE), 
				1, meta_cache_entry->fptr);
		}

		/* Reclaim key page */
		memset(&tmp_list_page, 0, sizeof(KEY_LIST_PAGE));
		tmp_list_page.next_list_pos = xattr_page->reclaimed_key_list_page;
		
		FSEEK(meta_cache_entry->fptr, reclaim_key_list_pos, SEEK_SET);
		FWRITE(&tmp_list_page, sizeof(KEY_LIST_PAGE), 1, 
			meta_cache_entry->fptr);
		
		xattr_page->reclaimed_key_list_page = reclaim_key_list_pos;
		FSEEK(meta_cache_entry->fptr, xattr_filepos, SEEK_SET);
		FWRITE(xattr_page, sizeof(XATTR_PAGE), 1, meta_cache_entry->fptr);
	}

	/* Recalim value block */
	ret_code = reclaim_replace_value_block(meta_cache_entry, xattr_page, 
		&first_value_pos);
	if (ret_code < 0)
		return ret_code;
	
	(xattr_page->namespace_page[name_space].num_xattr)--;	
	FSEEK(meta_cache_entry->fptr, xattr_filepos, SEEK_SET);
	FWRITE(xattr_page, sizeof(XATTR_PAGE), 1, meta_cache_entry->fptr);
	write_log(10, "Debug removexattr; Now reclaimed_key_page point to %lld, "
		"and reclaimed_value_block point to %lld\n", 
		xattr_page->reclaimed_key_list_page, 
		xattr_page->reclaimed_value_block);
		
	return 0;

errcode_handle:
	return errcode;
}



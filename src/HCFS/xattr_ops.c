/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: xattr_ops.c
* Abstract: The c source code file for xattr operations. The file includes
*           operation of insert, remove, get, and list xattr corresponding
*           to fuse operaion aout xattr.
*
* Revision History
* 2015/6/15 Kewei creates the file and add function parse_xattr_namespace()
* 2015/6/16 Kewei adds some functions about xattr_insert
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
		return -EINVAL;
	
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
		return -EINVAL; /* Namespace is not supported. */
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
	}

	*usable_pos = ret_pos;
	return 0;
}


int key_binary_search(KEY_ENTRY *key_list, unsigned num_xattr, const char *key, 
	int *index)
{
	unsigned start_index, end_index, mid_index;
	int cmp_result;
	
	start_index = 0;
	end_index = num_xattr;
	while (end_index > start_index) {
		mid_index = (end_index + start_index) / 2;

		cmp_result = strcmp(key, key_list[mid_index].key);

		if (cmp_result == 0) { /* Hit key entry */
			*index = mid_index;
			return 0;
		}

		if (cmp_result < 0)
			end_index = mid_index;
		else
			start_index = mid_index;
	}
	
	/* Key entry not found */
	*index = -1;
	return -1;
}


int find_key_entry(META_CACHE_ENTRY_STRUCT *meta_cache_entry, 
	long long first_key_list_pos, KEY_LIST_PAGE *target_key_list_page, 
	int *key_index, long long *target_key_list_pos, const char *key)
{	
	long long key_list_pos;
	KEY_LIST_PAGE now_page;
	int ret;
	int index;
	char find_first_insert;
	char hit_key_entry;

	memset(&now_page, 0, sizeof(KEY_LIST_PAGE));
	key_list_pos = first_key_list_pos;
	find_first_insert = FALSE;
	hit_key_entry = FALSE;

	while (key_list_pos) {
		FSEEK(meta_cache_entry->fptr, key_list_pos, SEEK_SET);
		FREAD(&now_page, sizeof(KEY_LIST_PAGE), 1, meta_cache_entry->fptr);
		
		if (now_page.num_xattr >= MAX_KEY_ENTRY_PER_LIST) {
			key_list_pos = now_page.next_list_pos;
			continue;
		}
		
		ret = key_binary_search(now_page.key_list, now_page.num_xattr, key, &index);
		
		if (ret == 0) { /* Hit the key */
			hit_key_entry = TRUE;
			break;
		}
		
		/* Record first page which can be inserted */
		if ((find_first_insert == FALSE) && (ret < 0)) { 
			memcpy(target_key_list_page, &now_page, sizeof(KEY_LIST_PAGE));
			*key_index = index;
			*target_key_list_pos = key_list_pos;
			find_first_insert = TRUE; /* Just need the first one */
		}
		
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
		*target_key_list_pos = key_list_pos;
		return 1;
	}
	/* Hit nothing, but can insert into an entry but don't have to allocate
	   a new page, return the page which can be inserted key. */
	if ((hit_key_entry == FALSE) && (find_first_insert == TRUE)) {
		return 1;
	}
}

int insert_xattr(META_CACHE_ENTRY_STRUCT *meta_cache_entry, XATTR_PAGE *xattr_page, 
	long long xattr_filepos, char name_space, const char *key, const char *value, 
	size_t size)
{
	unsigned hash_entry;
	NAMESPACE_PAGE *namespace_page;
	KEY_LIST_PAGE target_key_list_page;
	KEY_LIST_PAGE tmp_key_list_page;
	KEY_ENTRY *now_key_entry;
	KEY_ENTRY buf_key_list[MAX_KEY_ENTRY_PER_LIST];
	int ret_code;
	int key_index;
	long long first_key_list_pos;
	long long target_key_list_pos;
	long long tmp_key_list_pos;
	long long value_pos;

	/* Find the key entry */
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
		memset(&target_key_list_page, 0, sizeof(KEY_LIST_PAGE));
		value_pos = get_usable_value_filepos(); /* Get first value block */
		now_key_entry = &(target_key_list_page.key_list[0]);
		copy_key_entry(now_key_entry, key, value_pos, size); /* Copy entry */

		FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
		FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,  /* Write data */
			meta_cache_entry->fptr);

	} else { /* Find the entry and "CREATE" or "REPLACE" */
		first_key_list_pos = namespace_page->key_hash_table[hash_entry];

		ret_code = find_key_entry(meta_cache_entry, first_key_list_pos, 
				&target_key_list_page, &key_index, 
				&target_key_list_pos, key);


		if (ret_code > 0) { /* Hit nothing, create key and value */

			if (key_index < 0) { /* All key_list are full, allocate new one */
				long long usable_pos;

				ret_code = get_usable_key_list_filepos(meta_cache_entry, 
						xattr_page, &usable_pos);
				/* Link to end of key_list  */
				target_key_list_page.next_list_pos = usable_pos; 

				FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
				FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 
						1, meta_cache_entry->fptr);
				
				/* New page at end of the linked-key_list */
				memset(&target_key_list_page, 0, sizeof(KEY_LIST_PAGE));
				target_key_list_page.prev_list_pos = target_key_list_pos;	
				key_index = 0;

			} else { /* Hit nothing, but can insert to target_key_list_page */
				KEY_ENTRY *key_list;
				unsigned num_remaining;
				
				key_list = target_key_list_page.key_list;
				num_remaining = target_key_list_page.num_xattr - key_index;
				
				memcpy(buf_key_list, key_list + sizeof(KEY_ENTRY) * key_index,
					num_remaining);
				memcpy(key_list + sizeof(KEY_ENTRY) * (key_index + 1), 
					buf_key_list, num_remaining);
			}
			/* Insert key into target_key_list */
			
			value_pos = get_usable_value_filepos(); /* Get a usable value filepos */
			now_key_entry = &(target_key_list_page.key_list[key_index]);
			
			copy_key_entry(now_key_entry, key, value_pos, size); /* Copy entry */
			(target_key_list_page.num_xattr)++; /* # of xattr += 1 */
			
			FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
			FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,  /* Write data */
				meta_cache_entry->fptr);

		} else if (ret_code == 0) { /* Hit the key entry, replace value */
			/* Replace the value size and then rewrite. */
			target_key_list_page.key_list[key_index].value_size = size; 
			FSEEK(meta_cache_entry->fptr, target_key_list_pos, SEEK_SET);
			FWRITE(&target_key_list_page, sizeof(KEY_LIST_PAGE), 1,  /* Write data */
				meta_cache_entry->fptr);
			
			value_pos = target_key_list_page.key_list[key_index].first_value_block_pos;
		
		} else {
			return ret_code;
		}
	}

}



















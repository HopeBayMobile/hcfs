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

#include "pkg_cache.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "logger.h"
#include "params.h"
#include "utils.h"

/**
 * Hash package name.
 *
 * @return bucket index of hash table.
 */
static inline int32_t _pkg_hash(const char *input)
{
	/* FIXME: the string length can be calculated in advance. */
	return djb_hash(input, strlen(input)) & (PKG_HASH_SIZE - 1);
}

/**
 * Given pkg name, find the element in the linked list pointed by "entry_head".
 * If hit the pkg, "ret_now" will point to the entry, and ret_prev will point
 * to the previous entry corresponding to this entry. If hit nothing, then
 * "ret_now" points to last entry, and "ret_prev" points to previous entry
 * of last one.
 *
 * @return 0 when hit entry, otherwise return -ENOENT.
 */
static int32_t _lookup_pkg_list(PKG_ENTRY_HEAD *entry_head,
		const char *pkgname, PKG_CACHE_ENTRY **ret_prev,
		PKG_CACHE_ENTRY **ret_now)
{
	PKG_CACHE_ENTRY *now, *prev, *prev_prev;
	int32_t ret;

	prev_prev = NULL;
	prev = NULL;
	now = entry_head->first_pkg_entry;
	while (now) {
		if (!strcmp(pkgname, now->pkgname))
			break;
		prev_prev = prev;
		prev = now;
		now = now->next;
	}

	if (!now) {
		/* Last element */
		ret = -ENOENT;
		*ret_prev = prev_prev;
		*ret_now = prev;
	} else {
		ret = 0;
		*ret_prev = prev;
		*ret_now = now;
		pkg_cache.hit_count++;
	}
	pkg_cache.query_count++;

	return ret;
}

/**
 * Allocate a new pkg entry and init pkg name and uid. Then return the entry.
 *
 * @return pointer points to new entry. Otherwise return NULL.
 */
static PKG_CACHE_ENTRY *_new_pkg_entry(const char *pkgname, uid_t uid)
{
	PKG_CACHE_ENTRY *entry;

	entry = calloc(1, sizeof(PKG_CACHE_ENTRY));
	if (!entry) {
		return NULL;
	}
	snprintf(entry->pkgname, MAX_FILENAME_LEN, "%s", pkgname);
	entry->pkguid = uid;

	return entry;
}

/* *
 * Let pkg entry pointed by "now" be the MRU one. If prev is NULL,
 * it means "now" is the first element in the linked list.
 *
 * @return none.
 */
static void _promote_pkg_entry(PKG_ENTRY_HEAD *entry_head,
		PKG_CACHE_ENTRY *prev, PKG_CACHE_ENTRY *now)
{
	if (!now)
		return;	

	/* Prev points to next element of now. */
	if (prev) {
		prev->next = now->next;
		/* Be MRU one */
		now->next = entry_head->first_pkg_entry;
		entry_head->first_pkg_entry = now;
	} else {
		/* When prev is NULL, check first element */
		if (entry_head->first_pkg_entry != now) {
			now->next = entry_head->first_pkg_entry;
			entry_head->first_pkg_entry = now;
		}
	}
	return;
}

/**
 * Remove the entry "now" from linked list points by "entry_head".
 * "prev" is previous entry of "now" entry. If "prev" is NULL, check
 * whether "now" is first entry and remove it.
 *
 * @return none.
 */
static void _kick_entry(PKG_ENTRY_HEAD *entry_head,
		PKG_CACHE_ENTRY *prev, PKG_CACHE_ENTRY *now)
{
	if (!now)
		return;	

	if (prev) {
		prev->next = now->next;
	} else {
		if (entry_head->first_pkg_entry == now)
			entry_head->first_pkg_entry = now->next;
	}
	free(now);
	return;
}

/**
 * Initialize a package cache structure.
 *
 * @return none.
 */
int32_t init_pkg_cache(void)
{
	memset(&pkg_cache, 0, sizeof(PKG_CACHE));
	sem_init(&pkg_cache.pkg_cache_lock, 0, 1);
	return 0;
}

/**
 * Given a package name, lookup uid in pkg cache structure. If hit this
 * pkg entry, then let the entry be head of that linked list (MRU one).
 *
 * @return 0 on cache hit, otherwise -ENOENT.
 */
int32_t lookup_cache_pkg(const char *pkgname, uid_t *uid)
{
	int32_t ret;
	int32_t hash;
	PKG_CACHE_ENTRY *prev, *now;

	hash = _pkg_hash(pkgname);
	sem_wait(&pkg_cache.pkg_cache_lock);
	ret = _lookup_pkg_list(&(pkg_cache.pkg_hash[hash]),
			pkgname, &prev, &now);
	if (ret == 0) { /* Hit */
		*uid = now->pkguid;
		_promote_pkg_entry(&(pkg_cache.pkg_hash[hash]), prev, now);
		write_log(10, "Debug: Hit %s in pkg cache, hit rate %3lf\n",
				pkgname, (double)pkg_cache.hit_count /
				pkg_cache.query_count);
	}
	sem_post(&pkg_cache.pkg_cache_lock);

	return ret;
}

/**
 * Insert a pair of package name and uid to pkg cache structure. Check if
 * this entry had already been in this cache before insertion so that avoid
 * race condition. In case of entry not found, insert it to the head of
 * linked list and kick the LRU one (last one of that linked list).
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t insert_cache_pkg(const char *pkgname, uid_t uid)
{
	int32_t ret;
	int32_t hash;
	PKG_CACHE_ENTRY *entry, *prev, *now;

	hash = _pkg_hash(pkgname);
	sem_wait(&pkg_cache.pkg_cache_lock);
	ret = _lookup_pkg_list(&(pkg_cache.pkg_hash[hash]),
			pkgname, &prev, &now);
	if (ret == -ENOENT) {
		entry = _new_pkg_entry(pkgname, uid);
		if (!entry) {
			sem_post(&pkg_cache.pkg_cache_lock);
			write_log(0, "Error: Memory alloc error in %s\n",
					__func__);
			return -ENOMEM;
		}

		if (pkg_cache.pkg_hash[hash].num_pkgs == MAX_PKG_ENTRIES) {
			/* prev is second last entry and now is last entry.
			 * Kick the last one when list is full */
			_kick_entry(&(pkg_cache.pkg_hash[hash]),
					prev, now);
		} else {
			pkg_cache.pkg_hash[hash].num_pkgs += 1;
			pkg_cache.num_cache_pkgs += 1;
		}
		/* Add to head of list (MRU). */
		_promote_pkg_entry(&(pkg_cache.pkg_hash[hash]), NULL, entry);
		write_log(10, "Debug: %s miss in cache. Insert it.\n", pkgname);
	} else {
		/* Hit when insertion. Just let the pkg be MRU one. */
		_promote_pkg_entry(&(pkg_cache.pkg_hash[hash]), prev, now);
	}
	sem_post(&pkg_cache.pkg_cache_lock);

	return 0;
}

/**
 * Remove a package entry in cache. Do nothing when pkg is not found.
 *
 * @return 0 on success. -ENOENT when pkg not found.
 */
int32_t remove_cache_pkg(const char *pkgname)
{
	int32_t ret;
	int32_t hash;
	PKG_CACHE_ENTRY *prev, *now;

	hash = _pkg_hash(pkgname);
	sem_wait(&pkg_cache.pkg_cache_lock);
	ret = _lookup_pkg_list(&(pkg_cache.pkg_hash[hash]),
			pkgname, &prev, &now);
	if (ret == 0) {
		_kick_entry(&(pkg_cache.pkg_hash[hash]),
				prev, now);
		pkg_cache.pkg_hash[hash].num_pkgs -= 1;
		pkg_cache.num_cache_pkgs -= 1;
		write_log(10, "Debug: %s is removed from cache.\n", pkgname);
	}
	sem_post(&pkg_cache.pkg_cache_lock);

	return ret;
}

/**
 * Destroy and free all the pkg cache.
 *
 * @return 0 on success.
 */
int32_t destroy_pkg_cache(void)
{
	int32_t hash_idx;
	PKG_CACHE_ENTRY *now, *next;

	for (hash_idx = 0; hash_idx < PKG_HASH_SIZE; hash_idx++) {
		now = pkg_cache.pkg_hash[hash_idx].first_pkg_entry;
		while (now) {
			next = now->next;
			free(now);
			now = next;
			pkg_cache.pkg_hash[hash_idx].num_pkgs--;
			pkg_cache.num_cache_pkgs--;
		}
		pkg_cache.pkg_hash[hash_idx].first_pkg_entry = NULL;
	}
	return 0;
}

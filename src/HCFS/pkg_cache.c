#include "pkg_cache.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "params.h"

static inline int32_t _pkg_hash(const char *input)
{
	int32_t hash = 5381;
	int32_t index;

	index = 0;
	while (input[index]) {
		hash = (((hash << 5) + hash + input[index]) &
				(PKG_HASH_SIZE - 1));
		index++;
	}

	return hash;
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
	}

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

int32_t init_pkg_cache()
{
	memset(&pkg_cache, 0, sizeof(PKG_CACHE));
	sem_init(&pkg_cache.pkg_cache_lock, 0, 1);
	return 0;
}

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
	}
	sem_post(&pkg_cache.pkg_cache_lock);

	return ret;
}

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

	} else {
		/*  Let the pkg be MRU one.  */
		_promote_pkg_entry(&(pkg_cache.pkg_hash[hash]), prev, now);
	}
	sem_post(&pkg_cache.pkg_cache_lock);

	return 0;
}

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
	}
	sem_post(&pkg_cache.pkg_cache_lock);

	return ret;
}


int32_t destroy_pkg_cache()
{
	int32_t hash_idx;
	PKG_CACHE_ENTRY *now, *next;

	for (hash_idx = 0; hash_idx < PKG_HASH_SIZE; hash_idx++) {
		now = pkg_cache.pkg_hash[hash_idx].first_pkg_entry;
		while (now) {
			next = now->next;
			free(now);
			now = next;
		}
	}
	return 0;
}

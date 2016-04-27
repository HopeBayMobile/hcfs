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

static void _promote_pkg_entry(PKG_ENTRY_HEAD *entry_head,
		PKG_CACHE_ENTRY *prev, PKG_CACHE_ENTRY *now)
{
	if (!now)
		return;	

	if (prev) {
		prev->next = now->next;
		now->next = entry_head->first_pkg_entry;
		entry_head->first_pkg_entry = now;
	} else {
		now->next = entry_head->first_pkg_entry;
		entry_head->first_pkg_entry = now;
	}
	return;
}

static void _kick_entry(PKG_ENTRY_HEAD *entry_head,
		PKG_CACHE_ENTRY *prev, PKG_CACHE_ENTRY *now)
{
	if (!now)
		return;	

	if (prev)
		prev->next = now->next;
	else
		entry_head->first_pkg_entry = now->next;
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
	if (ret == 0)
		*uid = now->pkguid;
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

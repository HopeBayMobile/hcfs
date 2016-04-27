#ifndef GW20_HCFS_PKG_LOOKUP_H_
#define GW20_HCFS_PKG_LOOKUP_H_
#include <semaphore.h>

#define PKG_HASH_SIZE 8
#define MAX_PKG_ENTRIES 8
/* Moved pkg lookup here */
typedef struct {
	char pkgname[MAX_FILENAME_LEN+1];
	uid_t pkguid;
	struct PKG_CACHE_ENTRY *next;
} PKG_CACHE_ENTRY;

typedef struct {
	PKG_CACHE_ENTRY *first_pkg_entry;
	int num_pkgs;
} PKG_ENTRY_HEAD;

typedef struct {
	PKG_ENTRY_HEAD pkg_hash[PKG_HASH_SIZE];
	int num_cache_pkgs;
	sem_t pkg_cache_lock; /* Lock for package to uid lookup cache */
} PKG_CACHE;

PKG_CACHE pkg_lookup_cache;

int init_pkg_cache();
int destroy_pkg_cache();
int lookup_cache_pkg(const char *pkgname, uid_t *uid);
int insert_cache_pkg(const char *pkgname, uid_t uid);
int remove_cache_pkg(const char *pkgname);

#endif

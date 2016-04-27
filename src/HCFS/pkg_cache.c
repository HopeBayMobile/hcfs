#include <sys/types.h>
int init_pkg_cache()
{
	return 0;
}

int lookup_cache_pkg(const char *pkgname, uid_t *uid)
{
	return 0;
}

int insert_cache_pkg(const char *pkgname, uid_t uid)
{
	return 0;
}

int remove_cache_pkg(const char *pkgname)
{
	return 0;
}


int destroy_pkg_cache()
{
	return 0;
}

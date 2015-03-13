#include "gtest/gtest.h"

extern "C" {
#include "dir_lookup.h"
}

extern PATHNAME_CACHE_ENTRY pathname_cache[PATHNAME_CACHE_ENTRY_NUM];

/*
	Unittest of init_pathname_cache()
 */

TEST(init_pathname_cachetest, InitSuccess)
{
	PATHNAME_CACHE_ENTRY zero_entry;

	memset(&zero_entry, 0, sizeof(PATHNAME_CACHE_ENTRY));
	ASSERT_EQ(0, init_pathname_cache());
	for (int i = 0 ; i < PATHNAME_CACHE_ENTRY_NUM ; i++) {
		int val;
		sem_getvalue(&(pathname_cache[i].cache_entry_sem), &val);
		ASSERT_EQ(1, val);
		ASSERT_EQ(0, pathname_cache[i].inode_number);
	}
}

/*
	End of unittest of init_pathname_cache()
 */

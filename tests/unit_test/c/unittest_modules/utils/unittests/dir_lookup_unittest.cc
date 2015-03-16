#include "gtest/gtest.h"

extern "C" {
#include "dir_lookup.h"
}

extern PATHNAME_CACHE_ENTRY pathname_cache[PATHNAME_CACHE_ENTRY_NUM];

class DirLookupEnv : public ::testing::Environment {
	public:
		virtual void SetUp()
		{
			for (int i = 0 ; i < 2 * MAX_PATHNAME ; i++)
				long_path[i] = 'A';
			long_path[2 * MAX_PATHNAME - 1] = '\0';

		}
		virtual void TearDown()
		{

		}
		char long_path[2 * MAX_PATHNAME];
};

//::testing::Environment *const env = ::testing::AddGlobalTestEnvironment(new DirLookupEnv);
testing::AddGlobalTestEnvironment(new DirLookupEnv);

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

/*
	Unittest of replace_pathname_cache()
 */

TEST(replace_pathname_cacheTest, IndexOutOfBound)
{
	long long index = 5 * PATHNAME_CACHE_ENTRY_NUM;
	char *path = "/tmp/test";
	ino_t inode = 2;
	/* Test */
	EXPECT_EQ(-1, replace_pathname_cache(index, path, inode));
	index = PATHNAME_CACHE_ENTRY_NUM;
	EXPECT_EQ(-1, replace_pathname_cache(index, path, inode));
	index = -1;
	EXPECT_EQ(-1, replace_pathname_cache(index, path, inode));
}

TEST(replace_pathname_cacheTest, PathnameTooLong)
{
	ino_t inode = 2;
	long long index = 5;
	char path[2 * MAX_PATHNAME];
	/* Mock pathname */	
	for (int i = 0 ; i < 2 * MAX_PATHNAME ; i++)
		path[i] = 'A';
	path[2 * MAX_PATHNAME - 1] = '\0';
	/* Test */
	EXPECT_EQ(-1, replace_pathname_cache(index, path, inode));
	path[MAX_PATHNAME + 2] = '\0';
	EXPECT_EQ(-1, replace_pathname_cache(index, path, inode));
}

TEST(replace_pathname_cacheTest, RepalceSuccess)
{
	long long index = 5;
	char *path = "/tmp/test";
	ino_t inode = 2;
	/* Test */
	ASSERT_EQ(0, replace_pathname_cache(index, path, inode));
	EXPECT_STREQ(pathname_cache[index].pathname, path);
	EXPECT_EQ(pathname_cache[index].inode_number, inode);
}

/*
	End of unittest of replace_pathname_cache()
 */

/*
	Unittest of invalidate_pathname_cache_entry()
 */

TEST(invalidate_pathname_cache_entryTest, PathnameTooLong)
{
	char path[2 * MAX_PATHNAME];
	/* Mock pathname */	
	for (int i = 0 ; i < 2 * MAX_PATHNAME ; i++)
		path[i] = 'A';
	path[2 * MAX_PATHNAME - 1] = '\0';
	/* Test */
	EXPECT_EQ(-1, invalidate_pathname_cache_entry(path));
}

TEST(invalidate_pathname_cache_entryTest, PathNotInCache)
{
	unsigned long long index;
	char *path = "/tmp/test";
	/* Mock data */
	index = compute_hash(path);
	strcpy(pathname_cache[index].pathname, "/tmp/mock_path");
	pathname_cache[index].inode_number = 5;
	/* Test */
	EXPECT_EQ(0, invalidate_pathname_cache_entry(path));
	EXPECT_EQ(5, pathname_cache[index].inode_number);
	EXPECT_STREQ("/tmp/mock_path", pathname_cache[index].pathname);
}

TEST(invalidate_pathname_cache_entryTest, InvalidateCacheSuccess)
{
	unsigned long long index;
	char *path = "/tmp/test";
	/* Mock data */
	index = compute_hash(path);
	strcpy(pathname_cache[index].pathname, path);
	pathname_cache[index].inode_number = 5;
	/* Test */
	EXPECT_EQ(0, invalidate_pathname_cache_entry(path));
	EXPECT_EQ(0, pathname_cache[index].inode_number);
	EXPECT_STREQ("", pathname_cache[index].pathname);
}

/*
	End of unittest of invalidate_pathname_cache_entry()
 */

/*
	Unittest of check_cached_path()
 */

TEST(check_cached_pathTest, PathnameTooLong)
{
	/* Test */
	EXPECT_EQ(0, check_cached_path(env->long_path));
}

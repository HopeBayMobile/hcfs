#include "gtest/gtest.h"

extern "C" {
#include "dir_lookup.h"
}

extern PATHNAME_CACHE_ENTRY pathname_cache[PATHNAME_CACHE_ENTRY_NUM];

class DirLookupEnv : public ::testing::Environment {
	public:
		char long_path[2 * MAX_PATHNAME];
		virtual void SetUp()
		{
			for (int i = 0 ; i < 2 * MAX_PATHNAME ; i++)
				long_path[i] = 'A';
			long_path[2 * MAX_PATHNAME - 1] = '\0';

		}
		virtual void TearDown()
		{

		}
};

DirLookupEnv *env;

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
	/* Test */
	EXPECT_EQ(-1, replace_pathname_cache(index, env->long_path, inode));
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
	/* Test */
	EXPECT_EQ(-1, invalidate_pathname_cache_entry(env->long_path));
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

TEST(check_cached_pathTest, PathNotInCache)
{
	unsigned long long index;
	char *path = "/tmp/test";
	/* Mock data */
	index = compute_hash(path);
	strcpy(pathname_cache[index].pathname, "/tmp/mock_path");
	pathname_cache[index].inode_number = 5;
	/* Test */
	EXPECT_EQ(0, check_cached_path(path));
}

TEST(check_cached_pathTest, CheckCacheSuccess)
{
	unsigned long long index;
	/* Test for 500 times */	
	for (int times = 0 ; times < 500 ; times++) {
		char path[30];
		sprintf(path, "/tmp/test%d", times);
		/* Mock data */
		index = compute_hash(path);
		strcpy(pathname_cache[index].pathname, path);
		pathname_cache[index].inode_number = 5 * times;
		/* Test */
		ASSERT_EQ(5 * times, check_cached_path(path));
	}
}

/*
	End of unittest of check_cached_path()
 */

/*
	Unittest of lookup_pathname()
 */

TEST(lookup_pathnameTest, ErrorPathname)
{
	int errcode;
	EXPECT_EQ(0, lookup_pathname("error_path", &errcode));
	EXPECT_EQ(0, lookup_pathname("error_path/no_root", &errcode));
}

TEST(lookup_pathnameTest, LookupRootPathSuccess)
{
	int errcode;
	EXPECT_EQ(1, lookup_pathname("/", &errcode));
}

TEST(lookup_pathnameTest, PathFoundInCache)
{
	int errcode;
	unsigned long long index;
	ino_t test_inode = 123;
	char path[] = "/tmp/test1/test2/test3";
	
	/* init and mock data */
	ASSERT_EQ(0, init_pathname_cache());
	index = compute_hash(path);
	strcpy(pathname_cache[index].pathname, path);
	pathname_cache[index].inode_number = test_inode;
	/* Test */
	EXPECT_EQ(test_inode, lookup_pathname(path, &errcode));
}

/*
	End of unittest of lookup_pathname()
 */

int main(int argc, char *argv[])
{
	env = new DirLookupEnv;
	testing::AddGlobalTestEnvironment(env);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

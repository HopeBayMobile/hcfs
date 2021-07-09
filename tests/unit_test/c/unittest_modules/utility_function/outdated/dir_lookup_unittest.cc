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
#include "gtest/gtest.h"
#include "dir_lookup_params.h"
extern "C" {
#include "dir_lookup.h"
}

extern PATHNAME_CACHE_ENTRY pathname_cache[PATHNAME_CACHE_ENTRY_NUM];

class DirLookupEnv : public ::testing::Environment {
	public:
		char long_path[2 * MAX_PATHNAME];
		virtual void SetUp()
		{
			for (int32_t i = 0 ; i < 2 * MAX_PATHNAME ; i++)
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
	for (int32_t i = 0 ; i < PATHNAME_CACHE_ENTRY_NUM ; i++) {
		int32_t val;
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
	int64_t index = 5 * PATHNAME_CACHE_ENTRY_NUM;
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
	int64_t index = 5;
	/* Test */
	EXPECT_EQ(-1, replace_pathname_cache(index, env->long_path, inode));
}

TEST(replace_pathname_cacheTest, RepalceSuccess)
{
	int64_t index = 5;
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
	uint64_t index;
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
	uint64_t index;
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
	uint64_t index;
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
	uint64_t index;
	/* Test for 500 times */	
	for (int32_t times = 0 ; times < 500 ; times++) {
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

TEST(lookup_pathnameTest, LookupRootPathSuccess)
{
	int32_t errcode;
	EXPECT_EQ(1, lookup_pathname("/", &errcode));
}

TEST(lookup_pathnameTest, PathFoundInCache)
{
	int32_t errcode;
	uint64_t index;
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

TEST(lookup_pathnameTest, PrefixPathFoundInCache_With_Recursion_1_Layer)
{
	int32_t errcode;
	uint64_t index;
	char complete_path[] = "/tmp/file1/file2/file3";
	/* init and mock data */
	ASSERT_EQ(0, init_pathname_cache());
	index = compute_hash("/tmp/file1/file2");
	strcpy(pathname_cache[index].pathname, "/tmp/file1/file2");
	pathname_cache[index].inode_number = INO__FILE2_FOUND;
	/* Test */
	EXPECT_EQ(INO__FILE3_FOUND, lookup_pathname(complete_path, &errcode));
}

TEST(lookup_pathnameTest, PrefixPathFoundInCache_With_Recursion_MultiLayer)
{
	int32_t errcode;
	uint64_t index;
	char complete_path[] = "/tmp/file1/file2/file3/file4";
	/* init and mock data */
	ASSERT_EQ(0, init_pathname_cache());
	index = compute_hash("/tmp/file1");
	strcpy(pathname_cache[index].pathname, "/tmp/file1");
	pathname_cache[index].inode_number = INO__FILE1_FOUND;
	/* Test */
	EXPECT_EQ(INO__FILE4_FOUND, lookup_pathname(complete_path, &errcode));
}

TEST(lookup_pathnameTest, PrefixPathNotFoundInCache)
{
	int32_t errcode;
	uint64_t index;
	char complete_path[] = "/file1/file2/file3/file4";
	/* init and mock data */
	ASSERT_EQ(0, init_pathname_cache());
	/* Test */
	EXPECT_EQ(INO__FILE4_FOUND, lookup_pathname(complete_path, &errcode));
}

TEST(lookup_pathnameTest, FailToFindPrefixPath)
{
	int32_t errcode;
	/* init and mock data */
	ASSERT_EQ(0, init_pathname_cache());
	/* Test */
	EXPECT_EQ(0, lookup_pathname("/file1/file2/not_found", &errcode));
	EXPECT_EQ(0, lookup_pathname("/file1/file2/not_found/file3/file4", &errcode));
	EXPECT_EQ(0, lookup_pathname("/not_found/file1/file2/file3/file4", &errcode));
}

/*
	End of unittest of lookup_pathname()
 */

int32_t main(int32_t argc, char *argv[])
{
	env = new DirLookupEnv;
	testing::AddGlobalTestEnvironment(env);
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

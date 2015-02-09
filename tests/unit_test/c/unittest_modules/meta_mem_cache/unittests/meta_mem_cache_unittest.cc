#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "global.h"
#include "params.h"
#include "fuseop.h"
#include "dir_entry_btree.h"
#include "super_block.h"
#include "meta_mem_cache.h"
}
#include "gtest/gtest.h"
#include "mock_tool.h"

/* 
	Unit testing for meta_cache_open_file() 
 */

class meta_cache_open_fileTest : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			body_ptr = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		}

		virtual void TearDown() 
		{
			free(body_ptr);
		}
		META_CACHE_ENTRY_STRUCT *body_ptr;
};

TEST_F(meta_cache_open_fileTest, FetchMetaPathFail)
{
	/* Create a situation that fetch_meta_path fails. */
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = FETCH_META_PATH_FAIL; 
	/* Test */
	EXPECT_EQ(-1, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, FALSE);
}


TEST_F(meta_cache_open_fileTest, MetaPathCannotAccess)
{
	/* Create meta dir and meta file which cannot access. */
	mkdir(TMP_META_DIR, 0700);
	mknod(TMP_META_FILE_PATH, 0400, S_IFREG);
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = FETCH_META_PATH_SUCCESS; 
	/* Test */
	EXPECT_EQ(-1, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, FALSE);
	/* Delete tmp dir and file */
	ASSERT_EQ(0, unlink(TMP_META_FILE_PATH));
	ASSERT_EQ(0, rmdir(TMP_META_DIR));
}


TEST_F(meta_cache_open_fileTest, MetaPathNotExist)
{
	/* Create an empty meta dir. */
	mkdir(TMP_META_DIR, 0700);
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = FETCH_META_PATH_SUCCESS; 
	/* Test whether it created meta file. */
	EXPECT_EQ(0, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, TRUE);
	/* Delete meta file and dir */
	ASSERT_EQ(0, unlink(TMP_META_FILE_PATH));
	ASSERT_EQ(0, rmdir(TMP_META_DIR));
}


TEST_F(meta_cache_open_fileTest, MetaFileAlreadyOpened)
{
	/* Meta file has been opened*/
	body_ptr->meta_opened = TRUE;
	/* Test */	
	EXPECT_EQ(0, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, TRUE);
}


TEST_F(meta_cache_open_fileTest, OpenMetaPathSuccess)
{
	/* Create meta dir and meta file which cannot access. */
	mkdir(TMP_META_DIR, 0700);
	mknod(TMP_META_FILE_PATH, 0700, S_IFREG);
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = FETCH_META_PATH_SUCCESS; 
	/* Test */
	EXPECT_EQ(0, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, TRUE);
	/* Delete tmp dir and file */
	ASSERT_EQ(0, unlink(TMP_META_FILE_PATH));
	ASSERT_EQ(0, rmdir(TMP_META_DIR));
}

/* 
	End of unit testing for meta_cache_open_file() 
 */

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

TEST_F(meta_cache_open_fileTest, fetch_meta_pathFail)
{
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = FETCH_META_PATH_FAIL; 
	EXPECT_EQ(-1, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, FALSE);
}





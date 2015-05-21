extern "C" {
#include <fcntl.h>
#include "mock_param.h"
#include "super_block.h"
}
#include "gtest/gtest.h"

/*
	Unittest of write_super_block_head()
 */

TEST(write_super_block_headTest, WriteSuperBlockFail)
{
	char *sb_path = "testpatterns/mock_super_block";
	
	sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	sys_super_block->iofptr = open(sb_path, O_RDONLY, 0600);
	
	/* Run */
	EXPECT_EQ(-1, write_super_block_head());

	close(sys_super_block->iofptr);
	free(sys_super_block);
}

TEST(write_super_block_headTest, WriteSuperBlockSUCCESS)
{
	char *sb_path = "testpatterns/mock_super_block";
	
	sys_super_block = (SUPER_BLOCK_CONTROL *)malloc(sizeof(SUPER_BLOCK_CONTROL));
	sys_super_block->iofptr = open(sb_path, O_CREAT | O_RDWR, 0600);
	
	/* Run */
	EXPECT_EQ(0, write_super_block_head());

	close(sys_super_block->iofptr);
	free(sys_super_block);
	unlink(sb_path);
}

/*
	End of unittest of write_super_block_head()
 */

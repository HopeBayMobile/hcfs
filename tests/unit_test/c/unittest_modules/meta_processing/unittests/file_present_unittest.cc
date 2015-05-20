extern "C" {
#include "fuseop.h"
#include "file_present.h"
}
#include "gtest/gtest.h"
#include "mock_param.h"

/*
	Unittest of meta_forget_inode()
 */
TEST(meta_forget_inodeTest, RemoveMetaSucces)
{
	mknod(META_PATH, S_IFREG | 0700, 0);
	EXPECT_EQ(0, meta_forget_inode(5));

	EXPECT_EQ(-1, access(META_PATH, F_OK));
}
/*
	End of unittest of meta_forget_inode()
 */

/*
	Unittest of fetch_inode_stat()
 */

TEST(fetch_inode_statTest, FetchRootStatFail)
{
	ino_t inode = 0;
}

/*
	End of unittest of fetch_inode_stat()
 */

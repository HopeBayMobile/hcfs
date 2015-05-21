extern "C" {
#include "fuseop.h"
#include "file_present.h"
#include <errno.h>
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
	struct stat inode_stat;
	ino_t inode = 0;

	EXPECT_EQ(-ENOENT, fetch_inode_stat(inode, &inode_stat, NULL));
}

TEST(fetch_inode_statTest, FetchInodeStatSuccess)
{
	struct stat inode_stat;
	ino_t inode = INO_REGFILE;

	/* Run */
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, NULL));

	/* Verify */
	EXPECT_EQ(inode, inode_stat.st_ino);
	EXPECT_EQ(NUM_BLOCKS*MOCK_BLOCK_SIZE, inode_stat.st_size);
}

TEST(fetch_inode_statTest, FetchRegFileGenerationSuccess)
{
	struct stat inode_stat;
	unsigned long gen;
	ino_t inode = INO_REGFILE;

	gen = 0;

	/* Run */
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, &gen));

	/* Verify */
	EXPECT_EQ(GENERATION_NUM, gen);	
}

/*
	End of unittest of fetch_inode_stat()
 */

/*
 	Unittest of mknod_update_meta()
 */

TEST(mknod_update_metaTest, FailTo_meta_cache_update_file_data)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_FILE_FAIL;
	ino_t parent_inode = 1;

	EXPECT_EQ(-EACCES, mknod_update_meta(self_inode, parent_inode, 
		"\0", NULL, 0));
}

TEST(mknod_update_metaTest, FailTo_dir_add_entry)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_FILE_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_FAIL;
	struct stat tmp_stat;

	tmp_stat.st_mode = S_IFREG;

	EXPECT_EQ(-EACCES, mknod_update_meta(self_inode, parent_inode, 
		"\0", &tmp_stat, 0));
}

TEST(mknod_update_metaTest, FunctionWorkSuccess)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_FILE_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_SUCCESS;
	struct stat tmp_stat;

	tmp_stat.st_mode = S_IFREG;

	EXPECT_EQ(0, mknod_update_meta(self_inode, parent_inode,
		"not_used", &tmp_stat, 0));
}

/*
 	End of unittest of mknod_update_meta()
 */

/*
	Unittest of mkdir_update_meta()
 */

TEST(mkdir_update_metaTest, FailTo_meta_cache_update_dir_data)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_DIR_FAIL;
	ino_t parent_inode = 1;

	EXPECT_EQ(-EACCES, mkdir_update_meta(self_inode, parent_inode, 
		"\0", NULL, 0));
}

TEST(mkdir_update_metaTest, FailTo_dir_add_entry)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_DIR_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_FAIL;
	struct stat tmp_stat;

	tmp_stat.st_mode = S_IFDIR;

	EXPECT_EQ(-EACCES, mkdir_update_meta(self_inode, parent_inode, 
		"\0", &tmp_stat, 0));
}

TEST(mkdir_update_metaTest, FunctionWorkSuccess)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_DIR_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_SUCCESS;
	struct stat tmp_stat;

	tmp_stat.st_mode = S_IFDIR;

	EXPECT_EQ(0, mkdir_update_meta(self_inode, parent_inode, 
		"\0", &tmp_stat, 0));
}

/*
	End of unittest of mkdir_update_meta()
 */

/*
	Unittest of unlink_update_meta()
 */

TEST(unlink_update_metaTest, FailTo_dir_remove_entry)
{
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_FAIL;
	ino_t self_inode = 1;

	EXPECT_EQ(-EACCES, unlink_update_meta(parent_inode, 
		self_inode, "\0"));
}

TEST(unlink_update_metaTest, FunctionWorkSuccess)
{
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;
	ino_t self_inode = 1;

	EXPECT_EQ(0, unlink_update_meta(parent_inode, 
		self_inode, "\0"));
}

/*
	End of unittest of unlink_update_meta()
 */

/*
	Unittest of rmdir_update_meta()
 */

TEST(rmdir_update_metaTest, ChildrenNonempty)
{	
	ino_t self_inode = INO_CHILDREN_IS_NONEMPTY;
	ino_t parent_inode = 1;

	EXPECT_EQ(-ENOTEMPTY, rmdir_update_meta(parent_inode, 
		self_inode, "\0"));
}

TEST(rmdir_update_metaTest, FailTo_dir_remove_entry)
{	
	ino_t self_inode = INO_CHILDREN_IS_EMPTY;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_FAIL;

	EXPECT_EQ(-EACCES, rmdir_update_meta(parent_inode, 
		self_inode, "\0"));
}

TEST(rmdir_update_metaTest, FunctionWorkSuccess)
{	
	ino_t self_inode = INO_CHILDREN_IS_EMPTY;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;

	EXPECT_EQ(0, rmdir_update_meta(parent_inode, 
		self_inode, "\0"));
}

/*
	End of unittest of rmdir_update_meta()
 */

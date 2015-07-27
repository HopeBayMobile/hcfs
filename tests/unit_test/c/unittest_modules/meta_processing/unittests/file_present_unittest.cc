extern "C" {
#include "fuseop.h"
#include "file_present.h"
#include "meta_mem_cache.h"
#include <errno.h>
}
#include "gtest/gtest.h"
#include "mock_param.h"

/*
	Unittest of meta_forget_inode()
 */
TEST(meta_forget_inodeTest, RemoveMetaSucces)
{
	mknod(MOCK_META_PATH, S_IFREG | 0700, 0);
	EXPECT_EQ(0, meta_forget_inode(5));

	EXPECT_EQ(-1, access(MOCK_META_PATH, F_OK));
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

TEST(fetch_inode_statTest, FetchDirGenerationSuccess)
{
	struct stat inode_stat;
	unsigned long gen;
	ino_t inode = INO_DIR;

	gen = 0;

	/* Run */
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, &gen));

	/* Verify */
	EXPECT_EQ(GENERATION_NUM, gen);
}

TEST(fetch_inode_statTest, FetchSymlinkGenerationSuccess)
{
	struct stat inode_stat;
	unsigned long gen;
	ino_t inode = INO_LNK;

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

	EXPECT_EQ(-1, mknod_update_meta(self_inode, parent_inode,
		"\0", NULL, 0));
}

TEST(mknod_update_metaTest, FailTo_dir_add_entry)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_FILE_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_FAIL;
	struct stat tmp_stat;

	tmp_stat.st_mode = S_IFREG;

	EXPECT_EQ(-1, mknod_update_meta(self_inode, parent_inode,
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

	EXPECT_EQ(-1, mkdir_update_meta(self_inode, parent_inode,
		"\0", NULL, 0));
}

TEST(mkdir_update_metaTest, FailTo_dir_add_entry)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_DIR_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_FAIL;
	struct stat tmp_stat;

	tmp_stat.st_mode = S_IFDIR;

	EXPECT_EQ(-1, mkdir_update_meta(self_inode, parent_inode,
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

TEST(unlink_update_metaTest, FailTo_dir_remove_entry_RegfileMeta)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_FAIL;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_REGFILE;
	mock_entry.d_type = D_ISREG;

	EXPECT_EQ(-1, unlink_update_meta(parent_inode, &mock_entry));
}

TEST(unlink_update_metaTest, UnlinkUpdateRegfileMetaSuccess)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_REGFILE;
	mock_entry.d_type = D_ISREG;

	EXPECT_EQ(0, unlink_update_meta(parent_inode, &mock_entry));
}

TEST(unlink_update_metaTest, FailTo_dir_remove_entry_SymlinkMeta)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_FAIL;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_LNK;
	mock_entry.d_type = D_ISLNK;

	EXPECT_EQ(-1, unlink_update_meta(parent_inode, &mock_entry));
}

TEST(unlink_update_metaTest, UnlinkUpdateSymlinkMetaSuccess)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_LNK;
	mock_entry.d_type = D_ISLNK;

	EXPECT_EQ(0, unlink_update_meta(parent_inode, &mock_entry));
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

	EXPECT_EQ(-1, rmdir_update_meta(parent_inode,
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

/*
 	Unittest of fetch_xattr_page()
 */
class fetch_xattr_pageTest : public ::testing::Test {
protected:
	META_CACHE_ENTRY_STRUCT *mock_meta_entry;
	XATTR_PAGE *mock_xattr_page;
	const char *mock_meta_path;

	void SetUp()
	{
		mock_meta_entry = (META_CACHE_ENTRY_STRUCT *)
			malloc(sizeof(META_CACHE_ENTRY_STRUCT));

		mock_xattr_page = (XATTR_PAGE *) malloc(sizeof(XATTR_PAGE));
		memset(mock_xattr_page, 0, sizeof(XATTR_PAGE));

		mock_meta_path = "/tmp/mock_meta";
		if (!access(mock_meta_path, F_OK))
			unlink(mock_meta_path);

		mock_meta_entry->fptr = fopen(mock_meta_path, "wr+");
		fseek(mock_meta_entry->fptr, 0, SEEK_SET);
		fwrite(&mock_xattr_page, sizeof(XATTR_PAGE),
			1, mock_meta_entry->fptr);
	}

	void TearDown()
	{
		if (mock_meta_entry->fptr)
			fclose(mock_meta_entry->fptr);

		if (!access(mock_meta_path, F_OK))
			unlink(mock_meta_path);

		if (mock_meta_entry)
			free(mock_meta_entry);

		if (mock_xattr_page)
			free(mock_xattr_page);
	}
};

TEST_F(fetch_xattr_pageTest, InodeNumLessThanZero_FetchFail)
{
	int ret;

	mock_meta_entry->inode_num = 0;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, NULL);

	EXPECT_EQ(-EINVAL, ret);
}

TEST_F(fetch_xattr_pageTest, XattrPageNULL)
{
	int ret;

	mock_meta_entry->inode_num = 1;
	ret = fetch_xattr_page(mock_meta_entry, NULL, NULL);

	EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(fetch_xattr_pageTest, FetchRegFileXattrSuccess)
{
	int ret;
	long long xattr_pos;

	mock_meta_entry->inode_num = INO_REGFILE;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(sizeof(XATTR_PAGE), xattr_pos);
}

TEST_F(fetch_xattr_pageTest, FetchExistRegFileXattrSuccess)
{
	int ret;
	long long xattr_pos;
	XATTR_PAGE expected_xattr_page;

	memset(&expected_xattr_page, 'k', sizeof(XATTR_PAGE));

	fseek(mock_meta_entry->fptr, sizeof(XATTR_PAGE), SEEK_SET);
	fwrite(&expected_xattr_page, sizeof(XATTR_PAGE), 1,
		mock_meta_entry->fptr);

	mock_meta_entry->inode_num = INO_REGFILE_XATTR_PAGE_EXIST;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(sizeof(XATTR_PAGE) , xattr_pos);
	EXPECT_EQ(0, memcmp(&expected_xattr_page, mock_xattr_page,
		sizeof(XATTR_PAGE)));
}

TEST_F(fetch_xattr_pageTest, FetchDirXattrSuccess)
{
	int ret;
	long long xattr_pos;

	mock_meta_entry->inode_num = INO_DIR;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(sizeof(XATTR_PAGE), xattr_pos);
}

TEST_F(fetch_xattr_pageTest, FetchExistDirXattrSuccess)
{
	int ret;
	long long xattr_pos;
	XATTR_PAGE expected_xattr_page;

	memset(&expected_xattr_page, 'k', sizeof(XATTR_PAGE));

	fseek(mock_meta_entry->fptr, sizeof(XATTR_PAGE), SEEK_SET);
	fwrite(&expected_xattr_page, sizeof(XATTR_PAGE), 1,
		mock_meta_entry->fptr);

	mock_meta_entry->inode_num = INO_DIR_XATTR_PAGE_EXIST;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(sizeof(XATTR_PAGE) , xattr_pos);
	EXPECT_EQ(0, memcmp(&expected_xattr_page, mock_xattr_page,
		sizeof(XATTR_PAGE)));
}

TEST_F(fetch_xattr_pageTest, FetchSymlinkXattrSuccess)
{
	int ret;
	long long xattr_pos;

	mock_meta_entry->inode_num = INO_LNK;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(sizeof(XATTR_PAGE), xattr_pos);
}

TEST_F(fetch_xattr_pageTest, FetchExistSymlinkXattrSuccess)
{
	int ret;
	long long xattr_pos;
	XATTR_PAGE expected_xattr_page;

	memset(&expected_xattr_page, 'k', sizeof(XATTR_PAGE));

	fseek(mock_meta_entry->fptr, sizeof(XATTR_PAGE), SEEK_SET);
	fwrite(&expected_xattr_page, sizeof(XATTR_PAGE), 1,
		mock_meta_entry->fptr);

	mock_meta_entry->inode_num = INO_LNK_XATTR_PAGE_EXIST;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(sizeof(XATTR_PAGE) , xattr_pos);
	EXPECT_EQ(0, memcmp(&expected_xattr_page, mock_xattr_page,
		sizeof(XATTR_PAGE)));
}
/*
 	End of unittest of fetch_xattr_page()
 */

/*
	Unittest of symlink_update_meta()
 */
class symlink_update_metaTest : public ::testing::Test {
protected:
	META_CACHE_ENTRY_STRUCT *mock_parent_entry;
	void SetUp()
	{
		mock_parent_entry = (META_CACHE_ENTRY_STRUCT *)
			malloc(sizeof(META_CACHE_ENTRY_STRUCT));
	}

	void TearDown()
	{
		if (mock_parent_entry)
			free(mock_parent_entry);
	}
};

TEST_F(symlink_update_metaTest, AddDirEntryFail)
{
	struct stat mock_stat;

	mock_stat.st_ino = 123;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_FAIL;

	EXPECT_EQ(-1, symlink_update_meta(mock_parent_entry, &mock_stat,
		"link_not_used", 12, "name_not_used"));
}

TEST_F(symlink_update_metaTest, SymlinkUpdateDataFail)
{
	struct stat mock_stat;

	mock_stat.st_ino = 123;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_SUCCESS;

	EXPECT_EQ(-1, symlink_update_meta(mock_parent_entry, &mock_stat,
		"update_symlink_data_fail", 12, "name_not_used"));
}

TEST_F(symlink_update_metaTest, UpdateMetaSuccess)
{
	struct stat mock_stat;

	mock_stat.st_ino = 123;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_SUCCESS;

	EXPECT_EQ(0, symlink_update_meta(mock_parent_entry, &mock_stat,
		"link_not_used", 12, "name_not_used"));

}
/*
	End of unittest of symlink_update_meta()
 */

/*
	Unittest of link_update_meta()
 */


/*
	End of unittest of link_update_meta()
 */

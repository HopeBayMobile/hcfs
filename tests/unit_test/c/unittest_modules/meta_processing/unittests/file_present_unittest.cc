extern "C" {
#include "fuseop.h"
#include "file_present.h"
#include "meta_mem_cache.h"
#include "global.h"
#include "path_reconstruct.h"
#include <errno.h>
}
#include "gtest/gtest.h"
#include "mock_param.h"
fuse_req_t req1;

class file_presentEnvironment : public ::testing::Environment {
	public:
		void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
			memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		}
		void TearDown()
		{
			free(system_config);
		}
};

::testing::Environment* const env =
	::testing::AddGlobalTestEnvironment(new file_presentEnvironment);


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

	EXPECT_EQ(-ENOENT, fetch_inode_stat(inode, &inode_stat, NULL, NULL));
}

TEST(fetch_inode_statTest, FetchInodeStatSuccess)
{
	struct stat inode_stat;
	ino_t inode = INO_REGFILE;

	/* Run */
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, NULL, NULL));

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
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, &gen, NULL));

	/* Verify */
	EXPECT_EQ(GENERATION_NUM, gen);
}

TEST(fetch_inode_statTest, FetchFIFOFileGenerationSuccess)
{
	struct stat inode_stat;
	unsigned long gen;
	ino_t inode = INO_FIFO;

	gen = 0;

	/* Run */
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, &gen, NULL));

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
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, &gen, NULL));

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
	EXPECT_EQ(0, fetch_inode_stat(inode, &inode_stat, &gen, NULL));

	/* Verify */
	EXPECT_EQ(GENERATION_NUM, gen);
}
/*
	End of unittest of fetch_inode_stat()
 */

/*
 	Unittest of mknod_update_meta()
 */
class mknod_update_metaTest : public ::testing::Test {
protected:
	void SetUp()
	{
		sem_init(&(pathlookup_data_lock), 0, 1);
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
	}

	void TearDown()
	{
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
	}
};

TEST_F(mknod_update_metaTest, FailTo_dir_add_entry)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_FILE_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_FAIL;
	struct stat tmp_stat;
	long long delta_metasize;

	tmp_stat.st_mode = S_IFREG;

	EXPECT_EQ(-1, mknod_update_meta(self_inode, parent_inode,
		"\0", &tmp_stat, 0, 1, &delta_metasize, TRUE));
}

TEST_F(mknod_update_metaTest, FailTo_meta_cache_update_file_data)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_FILE_FAIL;
	ino_t parent_inode = 1;
	struct stat tmp_stat;
	long long delta_metasize;

	tmp_stat.st_mode = S_IFREG;
	EXPECT_EQ(-1, mknod_update_meta(self_inode, parent_inode,
		"\0", &tmp_stat, 0, 1, &delta_metasize, TRUE));
}

TEST_F(mknod_update_metaTest, FunctionWorkSuccess)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_FILE_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_SUCCESS;
	struct stat tmp_stat;
	long long delta_metasize;
	int ret;

	tmp_stat.st_mode = S_IFREG;

	EXPECT_EQ(0, mknod_update_meta(self_inode, parent_inode,
		"not_used", &tmp_stat, 0, 1, &delta_metasize, TRUE));
}

/*
 	End of unittest of mknod_update_meta()
 */

/*
	Unittest of mkdir_update_meta()
 */
TEST(mkdir_update_metaTest, FailTo_dir_add_entry)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_DIR_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_FAIL;
	struct stat tmp_stat;
	long long delta_metasize;

	tmp_stat.st_mode = S_IFDIR;
	sem_init(&(pathlookup_data_lock), 0, 1);

	EXPECT_EQ(-1, mkdir_update_meta(self_inode, parent_inode,
		"\0", &tmp_stat, 0, 1, &delta_metasize, TRUE));
}

TEST(mkdir_update_metaTest, FailTo_meta_cache_update_dir_data)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_DIR_FAIL;
	ino_t parent_inode = 1;
	struct stat tmp_stat;
	long long delta_metasize;

	tmp_stat.st_mode = S_IFDIR;
	sem_init(&(pathlookup_data_lock), 0, 1);
	EXPECT_EQ(-1, mkdir_update_meta(self_inode, parent_inode,
		"\0", &tmp_stat, 0, 1, &delta_metasize, TRUE));
}

TEST(mkdir_update_metaTest, FunctionWorkSuccess)
{
	ino_t self_inode = INO_META_CACHE_UPDATE_DIR_SUCCESS;
	ino_t parent_inode = INO_DIR_ADD_ENTRY_SUCCESS;
	struct stat tmp_stat;
	long long delta_metasize;

	tmp_stat.st_mode = S_IFDIR;
	sem_init(&(pathlookup_data_lock), 0, 1);

	EXPECT_EQ(0, mkdir_update_meta(self_inode, parent_inode,
		"\0", &tmp_stat, 0, 1, &delta_metasize, TRUE));
}

/*
	End of unittest of mkdir_update_meta()
 */

/*
	Unittest of unlink_update_meta()
 */

class unlink_update_metaTest : public ::testing::Test {
protected:
	void SetUp()
	{
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
		sem_init(&(pathlookup_data_lock), 0, 1);
	}
	void TearDown()
	{
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
		sem_destroy(&(pathlookup_data_lock));
	}
};

TEST_F(unlink_update_metaTest, FailTo_dir_remove_entry_RegfileMeta)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_FAIL;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_REGFILE;
	mock_entry.d_type = D_ISREG;

	EXPECT_EQ(-1, unlink_update_meta(req1, parent_inode,
		&mock_entry));
}

TEST_F(unlink_update_metaTest, UnlinkUpdateRegfileMetaSuccess)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_REGFILE;
	mock_entry.d_type = D_ISREG;

	EXPECT_EQ(0, unlink_update_meta(req1, parent_inode,
			&mock_entry));
}

TEST_F(unlink_update_metaTest, UnlinkUpdateFIFOfileMetaSuccess)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_FIFO;
	mock_entry.d_type = D_ISFIFO;

	EXPECT_EQ(0, unlink_update_meta(req1, parent_inode,
			&mock_entry));
}

TEST_F(unlink_update_metaTest, FailTo_dir_remove_entry_SymlinkMeta)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_FAIL;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_LNK;
	mock_entry.d_type = D_ISLNK;

	EXPECT_EQ(-1, unlink_update_meta(req1, parent_inode,
			&mock_entry));
}

TEST_F(unlink_update_metaTest, UnlinkUpdateSymlinkMetaSuccess)
{
	DIR_ENTRY mock_entry;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;

	memset(&mock_entry, 0, sizeof(DIR_ENTRY));
	mock_entry.d_ino = INO_LNK;
	mock_entry.d_type = D_ISLNK;

	EXPECT_EQ(0, unlink_update_meta(req1, parent_inode,
			&mock_entry));
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
	sem_init(&(pathlookup_data_lock), 0, 1);

	EXPECT_EQ(-ENOTEMPTY, rmdir_update_meta(req1, parent_inode, 
		self_inode, "\0"));
}

TEST(rmdir_update_metaTest, FailTo_dir_remove_entry)
{
	ino_t self_inode = INO_CHILDREN_IS_EMPTY;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_FAIL;
	sem_init(&(pathlookup_data_lock), 0, 1);

	EXPECT_EQ(-1, rmdir_update_meta(req1, parent_inode, 
		self_inode, "\0"));
}

TEST(rmdir_update_metaTest, FunctionWorkSuccess)
{
	ino_t self_inode = INO_CHILDREN_IS_EMPTY;
	ino_t parent_inode = INO_DIR_REMOVE_ENTRY_SUCCESS;
	sem_init(&(pathlookup_data_lock), 0, 1);

	EXPECT_EQ(0, rmdir_update_meta(req1, parent_inode, 
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
		mock_meta_entry->meta_opened = TRUE;
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
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, NULL, FALSE);

	EXPECT_EQ(-EINVAL, ret);
}

TEST_F(fetch_xattr_pageTest, XattrPageNULL)
{
	int ret;

	mock_meta_entry->inode_num = 1;
	ret = fetch_xattr_page(mock_meta_entry, NULL, NULL, FALSE);

	EXPECT_EQ(-ENOMEM, ret);
}

TEST_F(fetch_xattr_pageTest, FetchFIFOfileXattr_EPERM)
{
	int ret;
	long long xattr_pos;

	xattr_pos = 0;
	mock_meta_entry->inode_num = INO_FIFO;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos, FALSE);

#ifdef _ANDROID_ENV_
	EXPECT_EQ(0, ret);
	EXPECT_EQ(sizeof(XATTR_PAGE), xattr_pos);
#else
	EXPECT_EQ(-EINVAL, ret);
	EXPECT_EQ(0, xattr_pos);
#endif
}

TEST_F(fetch_xattr_pageTest, FetchRegFileXattrSuccess)
{
	int ret;
	long long xattr_pos;

	mock_meta_entry->inode_num = INO_REGFILE;
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos, TRUE);

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
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos, FALSE);

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
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos, TRUE);

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
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos, FALSE);

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
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos, TRUE);

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
	ret = fetch_xattr_page(mock_meta_entry, mock_xattr_page, &xattr_pos, FALSE);

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
		memset(mock_parent_entry, 0, sizeof(META_CACHE_ENTRY_STRUCT));
		sem_init(&(pathlookup_data_lock), 0, 1);
	}

	void TearDown()
	{
		if (mock_parent_entry)
			free(mock_parent_entry);
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
	}
};

TEST_F(symlink_update_metaTest, AddDirEntryFail)
{
	struct stat mock_stat;
	long long delta_metasize;

	mock_stat.st_ino = 123;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_FAIL;

	EXPECT_EQ(-1, symlink_update_meta(mock_parent_entry, &mock_stat,
		"link_not_used", 12, "name_not_used", 1, &delta_metasize,
		TRUE));
}

TEST_F(symlink_update_metaTest, SymlinkUpdateDataFail)
{
	struct stat mock_stat;
	long long delta_metasize;

	mock_stat.st_ino = 123;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_SUCCESS;

	EXPECT_EQ(-1, symlink_update_meta(mock_parent_entry, &mock_stat,
		"update_symlink_data_fail", 12, "name_not_used", 1,
		&delta_metasize, TRUE));
}

TEST_F(symlink_update_metaTest, UpdateMetaSuccess)
{
	struct stat mock_stat;
	long long delta_metasize;

	mock_stat.st_ino = 123;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_SUCCESS;

	EXPECT_EQ(0, symlink_update_meta(mock_parent_entry, &mock_stat,
		"link_not_used", 12, "name_not_used", 1,
		&delta_metasize, TRUE));
}
/*
	End of unittest of symlink_update_meta()
 */

/*
	Unittest of link_update_meta()
 */
class link_update_metaTest : public ::testing::Test {
protected:
	META_CACHE_ENTRY_STRUCT *mock_parent_entry;
	void SetUp()
	{
		mock_parent_entry = (META_CACHE_ENTRY_STRUCT *)
			malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		memset(mock_parent_entry, 0, sizeof(META_CACHE_ENTRY_STRUCT));
		sem_init(&(pathlookup_data_lock), 0, 1);
	}

	void TearDown()
	{
		if (mock_parent_entry)
			free(mock_parent_entry);
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
	}
};

TEST_F(link_update_metaTest, HardlinkToDirFail)
{
	ino_t link_inode;
	struct stat link_stat;
	unsigned long gen;

	link_inode = INO_DIR;

	EXPECT_EQ(-EISDIR, link_update_meta(link_inode, "new_name_not_used",
		&link_stat, &gen, mock_parent_entry));
}

TEST_F(link_update_metaTest, TooManyLinks)
{
	ino_t link_inode;
	struct stat link_stat;
	unsigned long gen;

	link_inode = INO_TOO_MANY_LINKS;

	EXPECT_EQ(-EMLINK, link_update_meta(link_inode, "new_name_not_used",
		&link_stat, &gen, mock_parent_entry));
}

TEST_F(link_update_metaTest, UpdateMetaFail)
{
	ino_t link_inode;
	struct stat link_stat;
	unsigned long gen;

	link_inode = INO_META_CACHE_UPDATE_FILE_FAIL;

	EXPECT_EQ(-1, link_update_meta(link_inode, "new_name_not_used",
		&link_stat, &gen, mock_parent_entry));
}

TEST_F(link_update_metaTest, AddEntryToParentDirFail)
{
	ino_t link_inode;
	struct stat link_stat;
	unsigned long gen;

	link_inode = INO_REGFILE;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_FAIL;

	EXPECT_EQ(-1, link_update_meta(link_inode, "new_name_not_used",
		&link_stat, &gen, mock_parent_entry));

	/* Verify */
	EXPECT_EQ(1, link_stat.st_nlink); // Set to 1 in fetch_inode_stat()
}

TEST_F(link_update_metaTest, UpdateMetaSuccess)
{
	ino_t link_inode;
	struct stat link_stat;
	unsigned long gen;

	link_inode = INO_REGFILE;
	mock_parent_entry->inode_num = INO_DIR_ADD_ENTRY_SUCCESS;

	EXPECT_EQ(0, link_update_meta(link_inode, "new_name_not_used",
		&link_stat, &gen, mock_parent_entry));

	/* Verify */
	EXPECT_EQ(GENERATION_NUM, gen);
	EXPECT_EQ(2, link_stat.st_nlink); // Set to 1 in fetch_inode_stat()
}
/*
	End of unittest of link_update_meta()
 */

/* Unittest for pin_inode() */
class pin_inodeTest : public ::testing::Test {
protected:
	long long mock_reserved_size;

	void SetUp()
	{
		mock_reserved_size = 0;
		CACHE_HARD_LIMIT = 0; /* Let pinned size not available */
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->systemdata.pinned_size = 0;
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
	}
};

TEST_F(pin_inodeTest, FailIn_fetch_inode_stat)
{
	ino_t inode = 0;

	EXPECT_EQ(-ENOENT, pin_inode(inode, &mock_reserved_size));
}

TEST_F(pin_inodeTest, FailIn_change_pin_flag)
{
	ino_t inode = 1; /* this inode # will fail in change_pin_flag() */

	EXPECT_EQ(-ENOMEM, pin_inode(inode, &mock_reserved_size));
}

TEST_F(pin_inodeTest, PinRegfile_ENOSPC)
{
	ino_t inode = INO_REGFILE;

	CACHE_HARD_LIMIT = 0; /* Let pinned size not available */
	EXPECT_EQ(-ENOSPC, pin_inode(inode, &mock_reserved_size));
}

TEST_F(pin_inodeTest, PinRegfileSuccess)
{
	ino_t inode = INO_REGFILE;

	CACHE_HARD_LIMIT = NUM_BLOCKS * MOCK_BLOCK_SIZE * 2;
	EXPECT_EQ(0, pin_inode(inode, &mock_reserved_size));
}

TEST_F(pin_inodeTest, PinFIFOSuccess)
{
	ino_t inode = INO_FIFO;

	EXPECT_EQ(0, pin_inode(inode, &mock_reserved_size));
}

TEST_F(pin_inodeTest, PinSymlinkSuccess)
{
	ino_t inode = INO_LNK;

	EXPECT_EQ(0, pin_inode(inode, &mock_reserved_size));
}

TEST_F(pin_inodeTest, PinDirSuccess)
{
	ino_t inode = INO_DIR;

	collect_dir_children_flag = FALSE;
	EXPECT_EQ(0, pin_inode(inode, &mock_reserved_size));
}

TEST_F(pin_inodeTest, PinDirSuccess_ManyChildren)
{
	ino_t inode = INO_DIR;

	collect_dir_children_flag = TRUE;
	EXPECT_EQ(0, pin_inode(inode, &mock_reserved_size));
}

/* End of unittest for pin_inode() */

/* Unittest for unpin_inode() */
class unpin_inodeTest : public ::testing::Test {
protected:
	long long mock_reserved_size;

	void SetUp()
	{
		mock_reserved_size = 0;
		CACHE_HARD_LIMIT = 0; /* Let pinned size not available */
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
		if (!access(MOCK_META_PATH, F_OK))
			unlink(MOCK_META_PATH);
	}
};

TEST_F(unpin_inodeTest, FailIn_fetch_inode_stat)
{
	ino_t inode = 0;

	EXPECT_EQ(-ENOENT, unpin_inode(inode, &mock_reserved_size));
}

TEST_F(unpin_inodeTest, FailIn_change_pin_flag)
{
	ino_t inode = 1; /* this inode # will fail in change_pin_flag() */

	EXPECT_EQ(-ENOMEM, unpin_inode(inode, &mock_reserved_size));
}

TEST_F(unpin_inodeTest, UnpinRegfileSuccess)
{
	ino_t inode = INO_REGFILE;

	EXPECT_EQ(0, unpin_inode(inode, &mock_reserved_size));
}

TEST_F(unpin_inodeTest, UnpinFIFOSuccess)
{
	ino_t inode = INO_FIFO;

	EXPECT_EQ(0, unpin_inode(inode, &mock_reserved_size));
}

TEST_F(unpin_inodeTest, UnpinSymlinkSuccess)
{
	ino_t inode = INO_LNK;

	EXPECT_EQ(0, unpin_inode(inode, &mock_reserved_size));
}

TEST_F(unpin_inodeTest, UnpinDirSuccess)
{
	ino_t inode = INO_DIR;

	collect_dir_children_flag = FALSE;
	EXPECT_EQ(0, unpin_inode(inode, &mock_reserved_size));
}

TEST_F(unpin_inodeTest, UnpinDirSuccess_ManyChildren)
{
	ino_t inode = INO_DIR;

	collect_dir_children_flag = TRUE;
	EXPECT_EQ(0, unpin_inode(inode, &mock_reserved_size));
}
/* End of unittest for unpin_inode() */

/* Unittest for increase_pinned_size() */
class increase_pinned_sizeTest : public ::testing::Test {
protected:
	void SetUp()
	{
		CACHE_HARD_LIMIT = 0;
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
	}
};

TEST_F(increase_pinned_sizeTest, QuotaIsSufficient)
{
	long long quota_size;
	long long file_size;

	quota_size = 100;
	file_size = 20;
	hcfs_system->systemdata.pinned_size = 200;

	EXPECT_EQ(0, increase_pinned_size(&quota_size, file_size));
	EXPECT_EQ(100 - 20, quota_size);
	EXPECT_EQ(200, hcfs_system->systemdata.pinned_size);
}

TEST_F(increase_pinned_sizeTest, QuotaIsInsufficient_SystemSufficient)
{
	long long quota_size;
	long long file_size;

	quota_size = 10;
	file_size = 50;
	CACHE_HARD_LIMIT = 200; /* pinned space = 0.8 * hard_limit */
	hcfs_system->systemdata.pinned_size = 0;

	EXPECT_EQ(0, increase_pinned_size(&quota_size, file_size));
	EXPECT_EQ(0, quota_size);
	EXPECT_EQ((50 - 10), hcfs_system->systemdata.pinned_size);
}

TEST_F(increase_pinned_sizeTest, QuotaIsInsufficient_SystemInsufficient)
{
	long long quota_size;
	long long file_size;

	quota_size = 10;
	file_size = 50;
	CACHE_HARD_LIMIT = 20; /* pinned space = 0.8 * hard_limit */
	hcfs_system->systemdata.pinned_size = 0;

	EXPECT_EQ(-ENOSPC, increase_pinned_size(&quota_size, file_size));
	EXPECT_EQ(10, quota_size);
	EXPECT_EQ(0, hcfs_system->systemdata.pinned_size);
}
/* End of unittest for increase_pinned_size() */

/* Unittest for decrease_pinned_size() */
class decrease_pinned_sizeTest : public ::testing::Test {
protected:
	void SetUp()
	{
		CACHE_HARD_LIMIT = 0;
		hcfs_system = (SYSTEM_DATA_HEAD *)
			malloc(sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
	}
};

TEST_F(decrease_pinned_sizeTest, QuotaIsSufficient)
{
	long long quota_size;
	long long file_size;

	quota_size = 100;
	file_size = 20;
	hcfs_system->systemdata.pinned_size = 200;

	EXPECT_EQ(0, decrease_pinned_size(&quota_size, file_size));
	EXPECT_EQ(100 - 20, quota_size);
	EXPECT_EQ(200, hcfs_system->systemdata.pinned_size);
}

TEST_F(decrease_pinned_sizeTest, QuotaIsInsufficient)
{
	long long quota_size;
	long long file_size;

	quota_size = 10;
	file_size = 40;
	hcfs_system->systemdata.pinned_size = 200;

	EXPECT_EQ(0, decrease_pinned_size(&quota_size, file_size));
	EXPECT_EQ(0, quota_size);
	EXPECT_EQ(200 - (40 - 10), hcfs_system->systemdata.pinned_size);
}

/* End of unittest for decrease_pinned_size() */

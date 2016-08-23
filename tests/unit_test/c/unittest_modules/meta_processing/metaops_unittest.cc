#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fuse/fuse_lowlevel.h>
#include <ftw.h>

extern "C" {
#include "mock_param.h"

#include "utils.h"
#include "global.h"
#include "hfuse_system.h"
#include "fuseop.h"
#include "params.h"
#include "metaops.h"
#include "FS_manager.h"
#include "xattr_ops.h"
}
#include "gtest/gtest.h"

/* Global vars  */
static const ino_t self_inode = 10;
static const ino_t parent_inode = 5;

extern int32_t DELETE_DIR_ENTRY_BTREE_RESULT;
extern SYSTEM_CONF_STRUCT *system_config;

static int do_delete (const char *fpath, const struct stat *sb,
		int32_t tflag, struct FTW *ftwbuf)
{
	switch (tflag) {
		case FTW_D:
		case FTW_DNR:
		case FTW_DP:
			rmdir (fpath);
			break;
		default:
			unlink (fpath);
			break;
	}
	return (0);
}
class metaopsEnvironment : public ::testing::Environment {
	public:
		void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
			memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
			META_SPACE_LIMIT = 10000;

			system_config->max_cache_limit =
				(int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));

			system_config->max_pinned_limit =
				(int64_t*)calloc(NUM_PIN_TYPES, sizeof(int64_t));
			CURRENT_BACKEND = NONE;

		/* Mock system statistics */
		hcfs_system = (SYSTEM_DATA_HEAD*)malloc(sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
		hcfs_system->systemdata.system_size = MOCK_SYSTEM_SIZE;
		hcfs_system->systemdata.cache_size = MOCK_CACHE_SIZE;
		hcfs_system->systemdata.cache_blocks = MOCK_CACHE_BLOCKS;


		}
		void TearDown()
		{
			free(system_config);
			free(hcfs_system);
		}
};

::testing::Environment* const metaops_env =
	::testing::AddGlobalTestEnvironment(new metaopsEnvironment);

/*
	Unittest of init_dir_page()
 */
TEST(init_dir_pageTest, InitOK)
{
        int64_t pos = 1000;

	DIR_ENTRY_PAGE *temppage;

	temppage = (DIR_ENTRY_PAGE*)malloc(sizeof(DIR_ENTRY_PAGE));

	/* Run tested function */
	EXPECT_EQ(0, init_dir_page(temppage, self_inode, parent_inode, pos));

        /* Check */
	EXPECT_EQ(2, temppage->num_entries);

	EXPECT_EQ(self_inode, (temppage->dir_entries[0]).d_ino);
	EXPECT_STREQ(".", (temppage->dir_entries[0]).d_name);
	EXPECT_EQ(D_ISDIR, (temppage->dir_entries[0]).d_type);

	EXPECT_EQ(parent_inode, (temppage->dir_entries[1]).d_ino);
	EXPECT_STREQ("..", (temppage->dir_entries[1]).d_name);
	EXPECT_EQ(D_ISDIR, (temppage->dir_entries[1]).d_type);

	EXPECT_EQ(pos, temppage->this_page_pos);

	/* Reclaim resource */
	free(temppage);
}
/*
	End of unittest for init_dir_page()
 */

/*
	Unittest of dir_add_entry()
 */
class dir_add_entryTest : public ::testing::Test {
	protected:
		char *self_name;
		META_CACHE_ENTRY_STRUCT *body_ptr;
		char mock_metaname[200];

		virtual void SetUp()
		{
                       	strcpy(mock_metaname,
				"/tmp/mock_meta_used_in_dir_add_entry");
			self_name = "selfname";
			body_ptr = (META_CACHE_ENTRY_STRUCT *)calloc(
			    1, sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
			body_ptr->fptr = fopen(mock_metaname, "w+");
			/* Init meta & stat to be verified */
			memset(&to_verified_meta, 0, sizeof(FILE_META_TYPE));
			memset(&to_verified_stat, 0, sizeof(HCFS_STAT));
                }

                virtual void TearDown()
		{
			if (body_ptr->fptr)
				fclose(body_ptr->fptr);
			unlink(mock_metaname);
			free(body_ptr);
                }
};

TEST_F(dir_add_entryTest, NoLockError)
{
	EXPECT_EQ(-1, dir_add_entry(parent_inode, self_inode, 
		self_name, S_IFMT, body_ptr, FALSE));
}

TEST_F(dir_add_entryTest, insert_dir_entryFail)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;
	memset(&tmp_dir_page, 0, sizeof(DIR_ENTRY_PAGE));
	fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	sem_wait(&body_ptr->access_sem);

	/* Run */
	EXPECT_EQ(-1, dir_add_entry(parent_inode, INO_INSERT_DIR_ENTRY_FAIL,
		self_name, S_IFMT, body_ptr, FALSE));
}

TEST_F(dir_add_entryTest, AddRegFileSuccess_WithoutSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;
	memset(&tmp_dir_page, 0, sizeof(DIR_ENTRY_PAGE));
	fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	to_verified_meta.total_children = TOTAL_CHILDREN_NUM;
	to_verified_stat.nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);

	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITHOUT_SPLITTING, 
		self_name, S_IFREG, body_ptr, FALSE));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.nlink);
}

TEST_F(dir_add_entryTest, AddDirSuccess_WithoutSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;
	memset(&tmp_dir_page, 0, sizeof(DIR_ENTRY_PAGE));
	fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	to_verified_meta.total_children = TOTAL_CHILDREN_NUM;
	to_verified_stat.nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);

	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITHOUT_SPLITTING, 
		self_name, S_IFDIR, body_ptr, FALSE));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM + 1, to_verified_stat.nlink);
}

TEST_F(dir_add_entryTest, AddRegFileSuccess_WithSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;

	memset(&tmp_dir_page, 0, sizeof(DIR_ENTRY_PAGE));
	for (int32_t i = 0 ; i < 5 ; i++) // 5 pages
		fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

	to_verified_meta.total_children = TOTAL_CHILDREN_NUM;
	to_verified_stat.nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);

	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITH_SPLITTING, 
		self_name, S_IFREG, body_ptr, FALSE));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.nlink);
}

TEST_F(dir_add_entryTest, AddDirSuccess_WithSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;

	memset(&tmp_dir_page, 0, sizeof(DIR_ENTRY_PAGE));
	for (int32_t i = 0 ; i < 5 ; i++) // 5 pages
		fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);

	to_verified_meta.total_children = TOTAL_CHILDREN_NUM;
	to_verified_stat.nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);

	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITH_SPLITTING, 
		self_name, S_IFDIR, body_ptr, FALSE));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM + 1, to_verified_stat.nlink);
}

/*
	End of unittest for dir_add_entry()
 */

/*
	Unittest of dir_remove_entry()
 */
class dir_remove_entryTest : public ::testing::Test {
	protected:

		char *self_name;
		char *metapath;

		DIR_ENTRY_PAGE *testpage;
		META_CACHE_ENTRY_STRUCT *body_ptr;

		virtual void SetUp() {
			FILE *fp;

                        self_name = "selfname";
			metapath = "/tmp/dir_remove_entry_meta_file";

			testpage = (DIR_ENTRY_PAGE*)calloc(1, sizeof(DIR_ENTRY_PAGE));
			/* Create mock meta file */
			fp = fopen(metapath, "w+");
			fwrite(testpage, sizeof(DIR_ENTRY_PAGE), 1, fp); // mock root_page used to read
			fclose(fp);
			/* Init meta cache entry. It will be read and modified in the function. */
			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
			body_ptr->fptr = fopen(metapath, "r+");
			setbuf(body_ptr->fptr, NULL);
			body_ptr->meta_opened = TRUE;
			/* to_verified_meta & to_verified_stat will be modify in dir_remove_entry().
			   Use the global vars to verify result. */
			memset(&to_verified_meta, 0, sizeof(FILE_META_TYPE));
			memset(&to_verified_stat, 0, sizeof(HCFS_STAT));
			to_verified_meta.total_children = TOTAL_CHILDREN_NUM;
			to_verified_stat.nlink = LINK_NUM;
                }

                virtual void TearDown() {
			fclose(body_ptr->fptr);
			unlink(metapath);
			free(body_ptr);
			free(testpage);
                }
};

TEST_F(dir_remove_entryTest, NoLockError)
{
	EXPECT_EQ(-1, dir_remove_entry(parent_inode, self_inode, self_name, S_IFMT, body_ptr,
		  FALSE));
}

TEST_F(dir_remove_entryTest, BtreeDelFailed_RemoveEntryFail)
{
	/* Mock data to force btree deletion failed */
	sem_wait(&(body_ptr->access_sem));
	DELETE_DIR_ENTRY_BTREE_RESULT = 0;

	/* Run tested function */
	EXPECT_EQ(-1, dir_remove_entry(parent_inode, self_inode, self_name, S_IFMT, body_ptr,
		  FALSE));
		
	/* Verify */
	EXPECT_EQ(TOTAL_CHILDREN_NUM, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.nlink);
	sem_post(&(body_ptr->access_sem));
}

TEST_F(dir_remove_entryTest, RemoveDirSuccess)
{
	/* Mock data */
	sem_wait(&(body_ptr->access_sem));
	DELETE_DIR_ENTRY_BTREE_RESULT = 1;

	/* Run tested function */
	EXPECT_EQ(0, dir_remove_entry(parent_inode, self_inode, self_name, S_IFDIR, body_ptr,
		  FALSE));

	/* Verify */
	EXPECT_EQ(TOTAL_CHILDREN_NUM - 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM - 1, to_verified_stat.nlink);
	sem_post(&(body_ptr->access_sem));
}

TEST_F(dir_remove_entryTest, RemoveRegFileSuccess)
{
	/* Mock data */
	sem_wait(&(body_ptr->access_sem));
	DELETE_DIR_ENTRY_BTREE_RESULT = 1;

	/* Run tested function */
	EXPECT_EQ(0, dir_remove_entry(parent_inode, self_inode, self_name, S_IFREG, body_ptr,
	 	  FALSE));

	/* Verify */
	EXPECT_EQ(TOTAL_CHILDREN_NUM - 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.nlink);
	sem_post(&(body_ptr->access_sem));
}

TEST_F(dir_remove_entryTest, RemoveSymlinkSuccess)
{
	/* Mock data */
	sem_wait(&(body_ptr->access_sem));
	DELETE_DIR_ENTRY_BTREE_RESULT = 1;

	/* Run tested function */
	EXPECT_EQ(0, dir_remove_entry(parent_inode, self_inode, self_name, S_IFLNK, body_ptr,
		  FALSE));

	/* Verify */
	EXPECT_EQ(TOTAL_CHILDREN_NUM - 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.nlink);
	sem_post(&(body_ptr->access_sem));
}
/*
	End of unittest for dir_remove_entry()
 */

/*
	Unittest of change_parent_inode()
 */
class change_parent_inodeTest : public ::testing::Test {
	protected:

		ino_t parent_inode2;
		META_CACHE_ENTRY_STRUCT *body_ptr;
		virtual void SetUp() {
			parent_inode2 = 6;

			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		}
                virtual void TearDown() {
			free(body_ptr);
		}
};

TEST_F(change_parent_inodeTest, ChangeOK)
{
	EXPECT_EQ(0, change_parent_inode(INO_SEEK_DIR_ENTRY_OK, parent_inode, 
		parent_inode2, body_ptr, FALSE));
}

TEST_F(change_parent_inodeTest, DirEntryNotFound)
{
	EXPECT_EQ(-ENOENT, change_parent_inode(INO_SEEK_DIR_ENTRY_NOTFOUND, 
		parent_inode, parent_inode2, body_ptr, FALSE));
}

TEST_F(change_parent_inodeTest, ChangeFail)
{
	EXPECT_EQ(-1, change_parent_inode(INO_SEEK_DIR_ENTRY_FAIL, parent_inode, 
		parent_inode2, body_ptr, FALSE));
}
/*
	End of unittest for change_parent_inode()
 */

/*
	Unittest of change_dir_entry_inode()
 */
class change_dir_entry_inodeTest : public ::testing::Test {
protected:
	ino_t new_inode;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	virtual void SetUp() {
		new_inode = 6;

		body_ptr = (META_CACHE_ENTRY_STRUCT*)
			malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		memset(&to_verified_meta, 0, sizeof(FILE_META_TYPE));
		memset(&to_verified_stat, 0, sizeof(HCFS_STAT));
	}
	virtual void TearDown() {
		free(body_ptr);
	}
};

TEST_F(change_dir_entry_inodeTest, ChangeREGOK) 
{	
	EXPECT_EQ(0, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_OK, 
		"/mock/target/name", new_inode, S_IFREG, body_ptr, FALSE));
}

TEST_F(change_dir_entry_inodeTest, ChangeFIFOOK) 
{	
	EXPECT_EQ(0, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_OK, 
		"/mock/target/name", new_inode, S_IFIFO, body_ptr, FALSE));
}

TEST_F(change_dir_entry_inodeTest, ChangeLNKOK) 
{	
	EXPECT_EQ(0, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_OK, 
		"/mock/target/name", new_inode, S_IFLNK, body_ptr, FALSE));
}

TEST_F(change_dir_entry_inodeTest, ChangeDIROK) 
{	
	EXPECT_EQ(0, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_OK, 
		"/mock/target/name", new_inode, S_IFDIR, body_ptr, FALSE));
}

TEST_F(change_dir_entry_inodeTest, DirEntryNotFound)
{
	EXPECT_EQ(-ENOENT, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_NOTFOUND, 
		"/mock/target/name", new_inode, S_IFDIR, body_ptr, FALSE));
}

TEST_F(change_dir_entry_inodeTest, ChangeFail)
{
	EXPECT_EQ(-1, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_FAIL, 
		"/mock/target/name", new_inode, S_IFREG, body_ptr, FALSE));
}
/*
	End of unittest for change_parent_inode()
 */

/*
	Unittest of delete_inode_meta()
 */
class delete_inode_metaTest : public ::testing::Test {
protected:
	void SetUp()
	{

	}

	void TearDown()
	{
		unlink(TO_DELETE_METAPATH);
	}

	/* This function is used to check rename or move success  */
	bool is_file_diff(FILE *f1, FILE *f2)
	{
		if ((!f1) || (!f2))
			return true;

		bool is_diff = false;
		while (!feof(f1) || !feof(f2)) {
			char buf1[5000], buf2[5000];
			int32_t read_size1 = fread(buf1, 1, 4096, f1);
			int32_t read_size2 = fread(buf2, 1, 4096, f2);
			if ((read_size1 > 0) && (read_size1 == read_size2)) {
				if (memcmp(buf1, buf2, 4096) != 0) {
					is_diff = true;
					break;
				}
			} else
				break;
		}
		return is_diff;
	}

	/* copy pattern file to tested file */
	void cp_pattern_meta(char *path)
	{
		char filebuf[5000];
		FILE *tar = fopen(path, "w+");
		FILE *src = fopen("testpatterns/mock_meta_pattern", "r");

		fseek(src, 0, SEEK_SET);
		fseek(tar, 0, SEEK_SET);
		while (!feof(src)) {
			int32_t read_size = fread(filebuf, 1, 4096, src);
			if (read_size > 0)
				fwrite(filebuf, 1, read_size, tar);
			else
				break;
		}

		fclose(src);
		fclose(tar);

	}
};
TEST_F(delete_inode_metaTest, DirectlyRenameSuccess)
{
	FILE *todeletefptr, *expectedfptr;
	ino_t inode = INO_RENAME_SUCCESS;
	char path[200];

	/* Copy pattern_meta to a mock meta file. it will be renamed in
	   delete_inode_meta() */
	fetch_meta_path(path, INO_RENAME_SUCCESS);
	cp_pattern_meta(path);

	/* Run */
	EXPECT_EQ(0, delete_inode_meta(inode));

	/* Verify */
	EXPECT_EQ(0, access(TO_DELETE_METAPATH, F_OK)); // rename success
	EXPECT_EQ(-1, access(path, F_OK)); // Old path should be deleted

	todeletefptr = fopen(TO_DELETE_METAPATH, "r"); // copied content
	expectedfptr = fopen("testpatterns/mock_meta_pattern", "r"); // expected content

	EXPECT_FALSE(is_file_diff(todeletefptr, expectedfptr));

	/* Free resource */
	fclose(todeletefptr);
	fclose(expectedfptr);
	unlink(path);
	unlink(TO_DELETE_METAPATH);
}

TEST_F(delete_inode_metaTest, Error_ToDeleteMetaError)
{
	/* This inode number results in fetch_todelete_path() set pathname = "\0" */
	ino_t inode = INO_RENAME_FAIL;

	/* Run */
	EXPECT_EQ(-ENOENT, delete_inode_meta(inode));
}

TEST_F(delete_inode_metaTest, Error_MetaFileNotExist)
{
	ino_t inode = INO_RENAME_SUCCESS;

	/* Run */
	EXPECT_EQ(-ENOENT, delete_inode_meta(inode));
}

/*
	End of unittest of delete_inode_meta()
 */

/*
	Unittest of decrease_nlink_inode_file()
 */
class decrease_nlink_inode_fileTest : public ::testing::Test {
protected:
	ino_t root_inode;
	fuse_req_t req1;
	virtual void SetUp()
	{
		/* Mock user-defined parameters */
		MAX_BLOCK_SIZE = PARAM_MAX_BLOCK_SIZE;
	}

	virtual void TearDown()
	{
		if (!access("testpatterns/markdelete", F_OK))
			nftw("testpatterns/markdelete", do_delete, 20, FTW_DEPTH);
	}
};

TEST_F(decrease_nlink_inode_fileTest, InodeStillReferenced)
{
	EXPECT_EQ(0, decrease_nlink_inode_file(req1,
		INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_2));
}

TEST_F(decrease_nlink_inode_fileTest, meta_cache_lock_entryFail)
{
	EXPECT_EQ(-ENOMEM, decrease_nlink_inode_file(req1,
		INO_LOOKUP_FILE_DATA_OK_LOCK_ENTRY_FAIL));
}

TEST_F(decrease_nlink_inode_fileTest, MarkBlockFilesToDel)
{
	char metapath[METAPATHLEN];
	char thisblockpath[200];
	FILE *tmp_file;

	METAPATH = "testpatterns";  // Let disk_markdelete() success
	fetch_meta_path(metapath, INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel);
	tmp_file = fopen(metapath, "w");
	fclose(tmp_file);

	/* Test */
	EXPECT_EQ(0, decrease_nlink_inode_file(req1,
		INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel));

	/* Verify */
	EXPECT_EQ(MOCK_SYSTEM_SIZE, hcfs_system->systemdata.system_size);
	snprintf(thisblockpath, 200, "%s/markdelete/inode%d_%d", METAPATH,
		INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel, ROOT_INODE);
	EXPECT_EQ(0, access(thisblockpath, F_OK));

	EXPECT_EQ(0, unlink(metapath));
	EXPECT_EQ(0, unlink(thisblockpath));
}

/*
	Unittest of decrease_nlink_inode_file()
 */

/*
	Unittest of seek_page()
 */

class seek_pageTest : public ::testing::Test {
protected:
	char *metapath;
	META_CACHE_ENTRY_STRUCT *body_ptr;
	int64_t pointers_per_page[5]; // A lookup table to find pow(POINTERS_PER_PAGE, k)

	void SetUp()
	{
		FILE_META_TYPE empty_file_meta;
		memset(&empty_file_meta, 0, sizeof(FILE_META_TYPE));

		metapath = "testpatterns/seek_page_meta_file";
		body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		sem_init(&(body_ptr->access_sem), 0, 1);
		body_ptr->fptr = fopen(metapath, "w+");
		setbuf(body_ptr->fptr, NULL);

		fwrite(&empty_file_meta, sizeof(FILE_META_TYPE), 1, body_ptr->fptr);
		body_ptr->meta_opened = TRUE;

		pointers_per_page[0] = 1;
		for (int32_t i = 1 ; i < 5 ; i++) // build power table
			pointers_per_page[i] = pointers_per_page[i - 1] * POINTERS_PER_PAGE;
	}

	void TearDown()
	{
		if (body_ptr->fptr)
			fclose(body_ptr->fptr);
		unlink(metapath);
		free(body_ptr);
	}

	void write_mock_file_meta(int32_t deep, int64_t target_page, int64_t expected_pos)
	{
		int64_t tmp_target_page;
		PTR_ENTRY_PAGE ptr_entry_page;

		memset(&ptr_entry_page, 0, sizeof(PTR_ENTRY_PAGE)); // Init all entry as 0
		tmp_target_page = target_page;
		for (int32_t level = 0 ; level < deep ; level++)
			tmp_target_page -= pointers_per_page[level];
		for (int32_t level = 1 ; level <= deep ; level++) { // Write level 1, 2, 3 index
			int32_t level_index = (tmp_target_page) / pointers_per_page[deep - level];
			ptr_entry_page.ptr[level_index] = (level == deep ? expected_pos :
					sizeof(FILE_META_TYPE) + sizeof(PTR_ENTRY_PAGE) * level);
			fwrite(&ptr_entry_page, sizeof(PTR_ENTRY_PAGE), 1, body_ptr->fptr);
			ptr_entry_page.ptr[level_index] = 0; // Recover
			tmp_target_page = (tmp_target_page) % pointers_per_page[deep - level];
		}
	}

};

TEST_F(seek_pageTest, DirectPageSuccess)
{
	int64_t actual_pos;
	int64_t expected_pos = sizeof(FILE_META_TYPE);
	int64_t target_page = 0;

	body_ptr->inode_num = INO_DIRECT_SUCCESS;

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page(body_ptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

TEST_F(seek_pageTest, SingleIndirectPageSuccess)
{
	/* Mock data */
	PTR_ENTRY_PAGE ptr_entry_page;
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page = POINTERS_PER_PAGE / 2; // Medium of range(1, 1024)
	int64_t tmp_target_page = target_page - 1;

	body_ptr->inode_num = INO_SINGLE_INDIRECT_SUCCESS;
	memset(&ptr_entry_page, 0, sizeof(PTR_ENTRY_PAGE));
	ptr_entry_page.ptr[tmp_target_page] = expected_pos; // Set expected result
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	fwrite(&ptr_entry_page, sizeof(PTR_ENTRY_PAGE), 1, body_ptr->fptr);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page(body_ptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

TEST_F(seek_pageTest, DoubleIndirectPageSuccess)
{
	/* Mock data */
	PTR_ENTRY_PAGE ptr_entry_page;
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page, tmp_target_page;
	int32_t level1_index, level2_index;

	target_page = POINTERS_PER_PAGE / 2 *
		(POINTERS_PER_PAGE + 1); // Medium of range(1024, 1024^2)
	tmp_target_page = target_page -
		pointers_per_page[0] -
		pointers_per_page[1];

	body_ptr->inode_num = INO_DOUBLE_INDIRECT_SUCCESS;
	memset(&ptr_entry_page, 0, sizeof(PTR_ENTRY_PAGE));
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	// Set level 1 index
	level1_index = (tmp_target_page) / POINTERS_PER_PAGE;
	ptr_entry_page.ptr[level1_index] = sizeof(FILE_META_TYPE) + sizeof(PTR_ENTRY_PAGE);
	fwrite(&ptr_entry_page, sizeof(PTR_ENTRY_PAGE), 1, body_ptr->fptr);
	// Set level 2 answer index
	level2_index = (tmp_target_page) % POINTERS_PER_PAGE;
	ptr_entry_page.ptr[level1_index] = 0;
	ptr_entry_page.ptr[level2_index] = expected_pos;
	fwrite(&ptr_entry_page, sizeof(PTR_ENTRY_PAGE), 1, body_ptr->fptr);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page(body_ptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);

}

TEST_F(seek_pageTest, TripleIndirectPageSuccess)
{
	/* Mock data */
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page;

	target_page = (pointers_per_page[2] +
		pointers_per_page[3]) / 4 * 3; // In range(1024^2, 1024^3)
	body_ptr->inode_num = INO_TRIPLE_INDIRECT_SUCCESS;
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	write_mock_file_meta(3, target_page, expected_pos);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page(body_ptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

TEST_F(seek_pageTest, QuadrupleIndirectPageSuccess)
{
	/* Mock data */
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page;

	target_page = (pointers_per_page[3] +
		pointers_per_page[4]) / 4 * 3; // In range(1024^3, 1024^4)
	body_ptr->inode_num = INO_QUADRUPLE_INDIRECT_SUCCESS;
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	write_mock_file_meta(4, target_page, expected_pos);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page(body_ptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

/*
	Unittest of seek_page()
 */

/*
	Unittest of create_page()
 */

class create_pageTest : public ::testing::Test {
protected:
	char *metapath;
	int64_t pointers_per_page[5];
	META_CACHE_ENTRY_STRUCT *body_ptr;

	void SetUp()
	{
		FILE_META_TYPE empty_file_meta;

		hcfs_system->systemdata.system_meta_size = 0;
		metapath = "testpatterns/create_page_meta_file";
		body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		body_ptr->inode_num = INO_CREATE_PAGE_SUCCESS;
		sem_init(&(body_ptr->access_sem), 0, 1);

		body_ptr->fptr = fopen(metapath, "w+");
		setbuf(body_ptr->fptr, NULL);

		memset(&empty_file_meta, 0, sizeof(FILE_META_TYPE)); // All indirect are 0
		fwrite(&empty_file_meta, sizeof(FILE_META_TYPE), 1, body_ptr->fptr);
		body_ptr->meta_opened = TRUE;

		pointers_per_page[0] = 1;
		for (int32_t i = 1 ; i < 5 ; i++)
			pointers_per_page[i] = pointers_per_page[i - 1] * POINTERS_PER_PAGE;

	}
	void TearDown()
	{
		if (body_ptr->fptr)
			fclose(body_ptr->fptr);
		unlink(metapath);
		free(body_ptr);
	}
	/* Following function is used ot trace the indirect page and find final empty block.
	   Return true if block exist and all set 0. */
	bool verify_mock_file_meta(int32_t deep, int64_t target_page)
	{
		int64_t tmp_target_page;
		PTR_ENTRY_PAGE ptr_entry_page;
		BLOCK_ENTRY_PAGE created_block_page, zero_block_page;
		int32_t result;

		tmp_target_page = target_page;
		for (int32_t level = 0 ; level < deep ; level++)
			tmp_target_page -= pointers_per_page[level];

		fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
		for (int32_t level = 1 ; level <= deep ; level++) { // Write level 1, 2, 3 index
			int32_t level_index = tmp_target_page / pointers_per_page[deep - level];
			fread(&ptr_entry_page, sizeof(PTR_ENTRY_PAGE), 1, body_ptr->fptr);
			fseek(body_ptr->fptr, ptr_entry_page.ptr[level_index], SEEK_SET);
			printf("Test: level %d: ptr_page_index = %d, next_filepos = %lld\n",
				level, level_index, ptr_entry_page.ptr[level_index]);
			tmp_target_page = tmp_target_page % pointers_per_page[deep - level];
		}

		fread(&created_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, body_ptr->fptr);
		memset(&zero_block_page, 0, sizeof(BLOCK_ENTRY_PAGE));
		result = memcmp(&zero_block_page, &created_block_page, sizeof(BLOCK_ENTRY_PAGE));
		return result ? false : true;
	}

};

TEST_F(create_pageTest, NotLock)
{
	EXPECT_EQ(-1, create_page(body_ptr, 0));
}

TEST_F(create_pageTest, NegativeTargetPageError)
{
	sem_wait(&body_ptr->access_sem);
	EXPECT_EQ(-1, create_page(body_ptr, -1));
}

TEST_F(create_pageTest, DirectPageCreateFail_NoSpace)
{
	/* Mock data */
	int64_t target_page;
	int64_t result_pos;
	int64_t expected_pos;
	FILE_META_TYPE file_meta;

	hcfs_system->systemdata.system_meta_size = META_SPACE_LIMIT + 1;
	target_page = 0;
	expected_pos = sizeof(FILE_META_TYPE);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	result_pos = create_page(body_ptr, target_page);

	/* Verify */
	EXPECT_EQ(expected_pos, result_pos);
	sem_post(&body_ptr->access_sem);
	pread(fileno(body_ptr->fptr), &file_meta, sizeof(FILE_META_TYPE), 0);
	EXPECT_EQ(expected_pos, file_meta.direct);
}

TEST_F(create_pageTest, DirectPageCreateSuccess)
{
	/* Mock data */
	int64_t target_page;
	int64_t result_pos;
	int64_t expected_pos;
	FILE_META_TYPE file_meta;

	target_page = 0;
	expected_pos = sizeof(FILE_META_TYPE);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	result_pos = create_page(body_ptr, target_page);

	/* Verify */
	EXPECT_EQ(expected_pos, result_pos);
	sem_post(&body_ptr->access_sem);
	pread(fileno(body_ptr->fptr), &file_meta, sizeof(FILE_META_TYPE), 0);
	EXPECT_EQ(expected_pos, file_meta.direct);
}

TEST_F(create_pageTest, SingleIndirectPageCreateSuccess)
{
	/* Mock data */
	int64_t target_page;
	int64_t result_pos;
	int64_t expected_pos;

	target_page = pointers_per_page[1] / 2;
	expected_pos = sizeof(FILE_META_TYPE) + sizeof(PTR_ENTRY_PAGE);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	result_pos = create_page(body_ptr, target_page);

	/* Verify */
	EXPECT_EQ(expected_pos, result_pos);
	sem_post(&body_ptr->access_sem);
	EXPECT_EQ(true, verify_mock_file_meta(1, target_page));
}

TEST_F(create_pageTest, DoubleIndirectPageCreateSuccess)
{
	/* Mock data */
	int64_t target_page;
	int64_t result_pos;
	int64_t expected_pos;

	target_page = (pointers_per_page[1] + pointers_per_page[2]) / 4 * 3;
	expected_pos = sizeof(FILE_META_TYPE) + sizeof(PTR_ENTRY_PAGE) * 2;

	/* Run */
	sem_wait(&body_ptr->access_sem);
	result_pos = create_page(body_ptr, target_page);

	/* Verify */
	EXPECT_EQ(expected_pos, result_pos);
	sem_post(&body_ptr->access_sem);
	EXPECT_EQ(true, verify_mock_file_meta(2, target_page));
}

TEST_F(create_pageTest, TripleIndirectPageCreateSuccess)
{
	/* Mock data */
	int64_t target_page;
	int64_t result_pos;
	int64_t expected_pos;

	target_page = (pointers_per_page[2] + pointers_per_page[3]) / 4 * 3;
	expected_pos = sizeof(FILE_META_TYPE) + sizeof(PTR_ENTRY_PAGE) * 3;

	/* Run */
	sem_wait(&body_ptr->access_sem);
	result_pos = create_page(body_ptr, target_page);

	/* Verify */
	EXPECT_EQ(expected_pos, result_pos);
	sem_post(&body_ptr->access_sem);
	EXPECT_EQ(true, verify_mock_file_meta(3, target_page));
}

TEST_F(create_pageTest, QuadrupleIndirectPageCreateSuccess)
{
	/* Mock data */
	int64_t target_page;
	int64_t result_pos;
	int64_t expected_pos;

	target_page = (pointers_per_page[3] + pointers_per_page[4]) / 4 * 3;
	expected_pos = sizeof(FILE_META_TYPE) + sizeof(PTR_ENTRY_PAGE) * 4;

	/* Run */
	sem_wait(&body_ptr->access_sem);
	result_pos = create_page(body_ptr, target_page);

	/* Verify */
	EXPECT_EQ(expected_pos, result_pos);
	sem_post(&body_ptr->access_sem);
	EXPECT_EQ(true, verify_mock_file_meta(4, target_page));
}

/*
	End of unittest of create_page()
 */

/*
	Unittest of seek_page2()
 */

class seek_page2Test : public seek_pageTest {
	/* Similar to seek_page() */
};

TEST_F(seek_page2Test, DirectPageSuccess)
{
	int64_t actual_pos;
	int64_t expected_pos = sizeof(FILE_META_TYPE);
	int64_t target_page = 0;
	FILE_META_TYPE meta;

	meta.direct = sizeof(FILE_META_TYPE);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page2(&meta, body_ptr->fptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

TEST_F(seek_page2Test, SingleIndirectPageSuccess)
{
	/* Mock data */
	FILE_META_TYPE meta;
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page = pointers_per_page[1] / 4 * 3; // In range(1, 1024)

	meta.single_indirect = sizeof(FILE_META_TYPE);
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	write_mock_file_meta(1, target_page, expected_pos);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page2(&meta, body_ptr->fptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

TEST_F(seek_page2Test, DoubleIndirectPageSuccess)
{
	/* Mock data */
	FILE_META_TYPE meta;
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page = (pointers_per_page[1] +
		pointers_per_page[2]) / 4 * 3; // In range(1, 1024)

	meta.double_indirect = sizeof(FILE_META_TYPE);
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	write_mock_file_meta(2, target_page, expected_pos);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page2(&meta, body_ptr->fptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

TEST_F(seek_page2Test, TripleIndirectPageSuccess)
{
	/* Mock data */
	FILE_META_TYPE meta;
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page = (pointers_per_page[2] +
		pointers_per_page[3]) / 4 * 3; // In range(1, 1024)

	meta.triple_indirect = sizeof(FILE_META_TYPE);
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	write_mock_file_meta(3, target_page, expected_pos);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page2(&meta, body_ptr->fptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

TEST_F(seek_page2Test, QuadrupleIndirectPageSuccess)
{
	/* Mock data */
	FILE_META_TYPE meta;
	int64_t actual_pos;
	int64_t expected_pos = 5566;
	int64_t target_page = (pointers_per_page[3] +
		pointers_per_page[4]) / 4 * 3; // In range(1, 1024)

	meta.quadruple_indirect = sizeof(FILE_META_TYPE);
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
	write_mock_file_meta(4, target_page, expected_pos);

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page2(&meta, body_ptr->fptr, target_page, 0);

	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);
}

/*
	End of unittest of seek_page2()
 */

/*
	Unittest of actual_delete_inode()
 */

class actual_delete_inodeTest : public ::testing::Test {
protected:
	FILE *meta_fp;
	FILE *statfptr;
	FS_STAT_T tmp_stat;
	char statpath[100];

	void SetUp()
	{
		char markdelete_path[100];

		METAPATH = "testpatterns";
		CURRENT_BACKEND = NONE;
		hcfs_system = (SYSTEM_DATA_HEAD*)
				malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
		
		/* Make a mock markdelete-inode because disk_cleardelete will 
		   unlink the file. */	
		if (access("testpatterns/markdelete", F_OK) < 0)
			mkdir("testpatterns/markdelete", 0700);

		snprintf(markdelete_path, 100, "%s/markdelete/inode%d", METAPATH,
			INO_DELETE_FILE_BLOCK);
		mknod(markdelete_path, S_IFREG, 0);
		snprintf(statpath, 100, "%s/stat%d", METAPATH, ROOT_INODE);
		statfptr = fopen(statpath, "w");
		memset(&tmp_stat, 0, sizeof(FS_STAT_T));
		tmp_stat.num_inodes = 1;

		fwrite(&tmp_stat, sizeof(FS_STAT_T), 1, statfptr);
		fsync(fileno(statfptr));

		fclose(statfptr);
	}

	void TearDown()
	{
		char thismetapath[100];
		char thisblockpath[100];

		unlink(statpath);
		CURRENT_BACKEND = NONE;

		/* delete meta */
		fetch_meta_path(thismetapath, INO_DELETE_FILE_BLOCK);
		if (!access(thismetapath, F_OK))
			unlink(thismetapath);

		/* delete todelete_meta */
		fetch_todelete_path(thismetapath, INO_DELETE_FILE_BLOCK);
		if (!access(thismetapath, F_OK))
			unlink(thismetapath);

		/* delete markdelete_inode */
		snprintf(thisblockpath, 100, "%s/markdelete/inode%d", METAPATH,
			INO_DELETE_FILE_BLOCK);
		unlink(thisblockpath);
		nftw("testpatterns/markdelete", do_delete, 20, FTW_DEPTH);

		fetch_meta_path(thismetapath, INO_DELETE_DIR);
		if (!access(thismetapath, F_OK))
			unlink(thismetapath);
		fetch_meta_path(thismetapath, INO_DELETE_LNK);
		if (!access(thismetapath, F_OK))
			unlink(thismetapath);

		free(hcfs_system);
	}
};

/* Code is removed from actual_delete_inode
#ifdef _ANDROID_ENV_
TEST_F(actual_delete_inodeTest, FailIn_pathlookup_write_parent)
{
	MOUNT_T mount_t;

	pathlookup_write_parent_success = FALSE;
	EXPECT_EQ(-EIO, actual_delete_inode(INO_DELETE_DIR, D_ISDIR,
		ROOT_INODE, &mount_t));

	pathlookup_write_parent_success = TRUE;
}

TEST_F(actual_delete_inodeTest, FailIn_delete_pathcache_node)
{
	MOUNT_T mount_t;

	mount_t.volume_type = ANDROID_EXTERNAL;
	delete_pathcache_node_success = FALSE;
	EXPECT_EQ(-EINVAL, actual_delete_inode(INO_DELETE_DIR, D_ISDIR,
		ROOT_INODE, &mount_t));

	delete_pathcache_node_success = TRUE;
}
#endif
*/
TEST_F(actual_delete_inodeTest, DeleteDirSuccess_NoBackend)
{
	MOUNT_T mount_t;
	char meta_path[100];
	char todelete_path[100];

	CURRENT_BACKEND = NONE;
	fetch_todelete_path(todelete_path, INO_DELETE_DIR);
	fetch_meta_path(meta_path, INO_DELETE_DIR);
	mknod(meta_path, 0700, 0);

	/* Run & Verify */
	EXPECT_EQ(0, actual_delete_inode(INO_DELETE_DIR, D_ISDIR,
		ROOT_INODE, &mount_t));
	EXPECT_EQ(-1, access(meta_path, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-1, access(todelete_path, F_OK));
	EXPECT_EQ(ENOENT, errno);

	unlink(meta_path);
	unlink(todelete_path);
}

TEST_F(actual_delete_inodeTest, DeleteDirSuccess_BackendConn)
{
	MOUNT_T mount_t;
	char meta_path[100];
	char todelete_path[100];

	CURRENT_BACKEND = SWIFT;
	fetch_todelete_path(todelete_path, INO_DELETE_DIR);
	fetch_meta_path(meta_path, INO_DELETE_DIR);
	mknod(meta_path, 0700, 0);

	/* Run & Verify */
	EXPECT_EQ(0, actual_delete_inode(INO_DELETE_DIR, D_ISDIR,
		ROOT_INODE, &mount_t));
	EXPECT_EQ(-1, access(meta_path, F_OK));
	EXPECT_EQ(0, access(todelete_path, F_OK));

	unlink(meta_path);
	unlink(todelete_path);
}

TEST_F(actual_delete_inodeTest, mptr_is_null_DeleteDirSuccess_BackendConn)
{
	MOUNT_T mount_t;
	char meta_path[100];
	char todelete_path[100];

	CURRENT_BACKEND = SWIFT;
	fetch_todelete_path(todelete_path, INO_DELETE_DIR);
	fetch_meta_path(meta_path, INO_DELETE_DIR);
	mknod(meta_path, 0700, 0);

	/* Run & Verify */
	EXPECT_EQ(0, actual_delete_inode(INO_DELETE_DIR, D_ISDIR,
		ROOT_INODE, NULL));
	EXPECT_EQ(-1, access(meta_path, F_OK));
	EXPECT_EQ(0, access(todelete_path, F_OK));

	unlink(meta_path);
	unlink(todelete_path);
}

TEST_F(actual_delete_inodeTest, DeleteSymlinkSuccess_NoBackend)
{
	MOUNT_T mount_t;
	char meta_path[100];
	char todelete_path[100];

	CURRENT_BACKEND = NONE;
	fetch_todelete_path(todelete_path, INO_DELETE_LNK);
	fetch_meta_path(meta_path, INO_DELETE_LNK);
	mknod(meta_path, 0700, 0);

	/* Run & Verify */
	EXPECT_EQ(0, actual_delete_inode(INO_DELETE_LNK, D_ISLNK,
		ROOT_INODE, &mount_t));
	EXPECT_EQ(-1, access(meta_path, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-1, access(todelete_path, F_OK));
	EXPECT_EQ(ENOENT, errno);

	unlink(meta_path);
	unlink(todelete_path);
}

TEST_F(actual_delete_inodeTest, DeleteSymlinkSuccess_BackendConn)
{
	MOUNT_T mount_t;
	char meta_path[100];
	char todelete_path[100];

	CURRENT_BACKEND = SWIFT;
	fetch_todelete_path(todelete_path, INO_DELETE_LNK);
	fetch_meta_path(meta_path, INO_DELETE_LNK);
	mknod(meta_path, 0700, 0);

	/* Run & Verify */
	EXPECT_EQ(0, actual_delete_inode(INO_DELETE_LNK, D_ISLNK,
		ROOT_INODE, &mount_t));
	EXPECT_EQ(-1, access(meta_path, F_OK));
	EXPECT_EQ(0, access(todelete_path, F_OK));

	unlink(meta_path);
	unlink(todelete_path);
}

TEST_F(actual_delete_inodeTest, mptr_is_null_DeleteSymlinkSuccess_BackendConn)
{
	MOUNT_T mount_t;
	char meta_path[100];
	char todelete_path[100];

	CURRENT_BACKEND = SWIFT;
	fetch_todelete_path(todelete_path, INO_DELETE_LNK);
	fetch_meta_path(meta_path, INO_DELETE_LNK);
	mknod(meta_path, 0700, 0);

	/* Run & Verify */
	EXPECT_EQ(0, actual_delete_inode(INO_DELETE_LNK, D_ISLNK,
		ROOT_INODE, NULL));
	EXPECT_EQ(-1, access(meta_path, F_OK));
	EXPECT_EQ(0, access(todelete_path, F_OK));

	unlink(meta_path);
	unlink(todelete_path);
}

TEST_F(actual_delete_inodeTest, DeleteRegFileSuccess_NoBackend)
{
	char thisblockpath[100];
	char thismetapath[100];
	char todel_metapath[100];
	bool block_file_existed;
	BLOCK_ENTRY_PAGE block_entry_page;
	HCFS_STAT mock_stat;
	FILE_META_TYPE mock_meta;
	FILE *tmp_fp;
	ino_t mock_inode = INO_DELETE_FILE_BLOCK;
	MOUNT_T mount_t;
	ino_t mock_root;

	CURRENT_BACKEND = NONE;
	mock_root = 556677;
	/* Mock system init data & block data */
	MAX_BLOCK_SIZE = PARAM_MAX_BLOCK_SIZE;
	memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
	sem_init(&(hcfs_system->access_sem), 0, 1);
	hcfs_system->systemdata.system_size = MOCK_SYSTEM_SIZE;
	hcfs_system->systemdata.cache_size = MOCK_CACHE_SIZE;
	hcfs_system->systemdata.cache_blocks = MOCK_CACHE_BLOCKS;
	hcfs_system->systemdata.unpin_dirty_data_size = MOCK_CACHE_SIZE;
	hcfs_system->systemdata.pinned_size = MOCK_CACHE_SIZE;

	for (int32_t i = 0; i < NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, mock_inode, i);
		tmp_fp = fopen(thisblockpath, "w");
		fclose(tmp_fp);
	}

	// make mock meta file so that delete_inode_meta() will success
	/* Make mock meta file. Add truncated size */
	memset(&block_entry_page, 0, sizeof(BLOCK_ENTRY_PAGE));
	memset(&mock_stat, 0, sizeof(HCFS_STAT));
	memset(&mock_meta, 0, sizeof(FILE_META_TYPE));
	for (int32_t i = 0; i < NUM_BLOCKS; i++)
		block_entry_page.block_entries[i].status = ST_LDISK;
	mock_stat.size = NUM_BLOCKS * MAX_BLOCK_SIZE + TRUNC_SIZE;
	mock_stat.ino = mock_inode;
	mock_meta.local_pin = FALSE;
	mock_meta.direct = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);

	fetch_todelete_path(todel_metapath, INO_DELETE_FILE_BLOCK);
	fetch_meta_path(thismetapath, INO_DELETE_FILE_BLOCK);
	meta_fp = fopen(thismetapath, "w+");
	fseek(meta_fp, 0, SEEK_SET);
	fwrite(&mock_stat, sizeof(HCFS_STAT), 1, meta_fp);
	fwrite(&mock_meta, sizeof(FILE_META_TYPE), 1, meta_fp);
	fwrite(&block_entry_page, sizeof(BLOCK_ENTRY_PAGE), 1, meta_fp);
	fclose(meta_fp);

	/* Run */
	EXPECT_EQ(0, actual_delete_inode(mock_inode, D_ISREG,
		mock_root, &mount_t));

	/* Verify if block files are removed correctly */
	EXPECT_EQ(MOCK_SYSTEM_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS - TRUNC_SIZE,
		hcfs_system->systemdata.system_size);
	EXPECT_EQ(MOCK_CACHE_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS,
		hcfs_system->systemdata.cache_size);
	EXPECT_EQ(MOCK_CACHE_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS,
		hcfs_system->systemdata.unpin_dirty_data_size);
	EXPECT_EQ(MOCK_CACHE_SIZE, hcfs_system->systemdata.pinned_size);
	EXPECT_EQ(MOCK_CACHE_BLOCKS - NUM_BLOCKS,
		hcfs_system->systemdata.cache_blocks);

	block_file_existed = false;
	for (int32_t i = 0; i < NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, mock_inode, i);
		tmp_fp = fopen(thisblockpath, "r");
		if (tmp_fp) {
			block_file_existed = true;
			fclose(tmp_fp);
			unlink(thisblockpath);
		}
	}
	EXPECT_EQ(false, block_file_existed);

	EXPECT_EQ(-1, access(thismetapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-1, access(todel_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);

	/* Free resource */
	unlink(thismetapath);
}


TEST_F(actual_delete_inodeTest, DeleteRegFileSuccess)
{
	char thisblockpath[100];
	char thismetapath[100];
	char todel_metapath[100];
	bool block_file_existed;
	BLOCK_ENTRY_PAGE block_entry_page;
	HCFS_STAT mock_stat;
	FILE_META_TYPE mock_meta;
	FILE *tmp_fp;
	ino_t mock_inode = INO_DELETE_FILE_BLOCK;
	MOUNT_T mount_t;
	ino_t mock_root;

	CURRENT_BACKEND = SWIFT;
	mock_root = 556677;
	/* Mock system init data & block data */
	MAX_BLOCK_SIZE = PARAM_MAX_BLOCK_SIZE;
	memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
	sem_init(&(hcfs_system->access_sem), 0, 1);
	hcfs_system->systemdata.system_size = MOCK_SYSTEM_SIZE;
	hcfs_system->systemdata.cache_size = MOCK_CACHE_SIZE;
	hcfs_system->systemdata.cache_blocks = MOCK_CACHE_BLOCKS;
	hcfs_system->systemdata.unpin_dirty_data_size = MOCK_CACHE_SIZE;
	hcfs_system->systemdata.pinned_size = MOCK_CACHE_SIZE;

	for (int32_t i = 0; i < NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, mock_inode, i);
		tmp_fp = fopen(thisblockpath, "w");
		fclose(tmp_fp);
	}

	// make mock meta file so that delete_inode_meta() will success
	/* Make mock meta file. Add truncated size */
	memset(&block_entry_page, 0, sizeof(BLOCK_ENTRY_PAGE));
	memset(&mock_stat, 0, sizeof(HCFS_STAT));
	memset(&mock_meta, 0, sizeof(FILE_META_TYPE));
	for (int32_t i = 0; i < NUM_BLOCKS; i++)
		block_entry_page.block_entries[i].status = ST_LDISK;
	mock_stat.size = NUM_BLOCKS * MAX_BLOCK_SIZE + TRUNC_SIZE;
	mock_stat.ino = mock_inode;
	mock_meta.local_pin = FALSE;
	mock_meta.direct = sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE);

	fetch_todelete_path(todel_metapath, INO_DELETE_FILE_BLOCK);
	fetch_meta_path(thismetapath, INO_DELETE_FILE_BLOCK);

	meta_fp = fopen(thismetapath, "w+");
	setbuf(meta_fp, NULL);
	fseek(meta_fp, 0, SEEK_SET);
	fwrite(&mock_stat, sizeof(HCFS_STAT), 1, meta_fp);
	fwrite(&mock_meta, sizeof(FILE_META_TYPE), 1, meta_fp);
	fwrite(&block_entry_page, sizeof(BLOCK_ENTRY_PAGE), 1, meta_fp);
	fclose(meta_fp);

	/* Run */
	EXPECT_EQ(0, actual_delete_inode(mock_inode, D_ISREG,
		mock_root, &mount_t));

	/* Verify if block files are removed correctly */
	EXPECT_EQ(MOCK_SYSTEM_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS -
		TRUNC_SIZE - MOCK_META_SIZE,
		hcfs_system->systemdata.system_size);
	EXPECT_EQ(MOCK_CACHE_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS - MOCK_META_SIZE,
		hcfs_system->systemdata.cache_size);
	EXPECT_EQ(MOCK_CACHE_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS,
		hcfs_system->systemdata.unpin_dirty_data_size);
	EXPECT_EQ(MOCK_CACHE_SIZE, hcfs_system->systemdata.pinned_size);
	EXPECT_EQ(MOCK_CACHE_BLOCKS - NUM_BLOCKS,
		hcfs_system->systemdata.cache_blocks);

	block_file_existed = false;
	for (int32_t i = 0; i < NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, mock_inode, i);
		tmp_fp = fopen(thisblockpath, "r");
		if (tmp_fp) {
			block_file_existed = true;
			fclose(tmp_fp);
			unlink(thisblockpath);
		}
	}
	EXPECT_EQ(false, block_file_existed);

	EXPECT_EQ(-1, access(thismetapath, F_OK));
	EXPECT_EQ(0, access(todel_metapath, F_OK));

	/* Free resource */
	unlink(thismetapath);
}

/*
	End of unittest of actual_delete_inode()
 */

/*
	Unittest of disk_markdelete()
 */

TEST(disk_markdeleteTest, MakeDir_markdelete_Fail)
{
	MOUNT_T tmpmount;

	METAPATH = "\0";
	tmpmount.f_ino = 556677;

	EXPECT_EQ(-EACCES, disk_markdelete(6, &tmpmount));
}

TEST(disk_markdeleteTest, MarkSuccess)
{
	MOUNT_T tmpmount;

	METAPATH = "/tmp";
	tmpmount.f_ino = 556677;

	/* Run */
	EXPECT_EQ(0, disk_markdelete(6, &tmpmount));

	/* Verify */
	EXPECT_EQ(0, access("/tmp/markdelete", F_OK));
	EXPECT_EQ(0, access("/tmp/markdelete/inode6_556677", F_OK));

	unlink("/tmp/markdelete/inode6_556677");
	nftw("/tmp/markdelete", do_delete, 20, FTW_DEPTH);
}

/*
	End of unittest of disk_markdelete()
 */

/*
	Unittest of disk_cleardelete()
 */

class disk_cleardeleteTest : public ::testing::Test {
protected:
	void SetUp()
	{
		mkdir("/tmp/markdelete", 0700);
	}

	void TearDown()
	{
		if (!access("/tmp/markdelete", F_OK))
			nftw("/tmp/markdelete", do_delete, 20, FTW_DEPTH);
	}
};

TEST_F(disk_cleardeleteTest, Dir_markdelete_NotExist)
{
	ino_t root_inode;

	METAPATH = "/adkas"; // Let access fail

	EXPECT_EQ(-ENOENT, disk_cleardelete(6, ROOT_INODE));
}

TEST_F(disk_cleardeleteTest, InodeNotExist)
{
	ino_t root_inode;

	METAPATH = "/tmp"; // inode6 does not exist in /tmp/markdelete

	EXPECT_EQ(0, disk_cleardelete(6, ROOT_INODE));
}

TEST_F(disk_cleardeleteTest, ClearInodeSuccess)
{
	char path[100];

	METAPATH = "/tmp";
	sprintf(path, "/tmp/markdelete/inode6_%d", ROOT_INODE);
	mknod(path, S_IFREG | 0700, 0);

	EXPECT_EQ(0, disk_cleardelete(6, ROOT_INODE));
	EXPECT_EQ(-1, access(path, F_OK));

	if (access(path, F_OK) == -1)
		unlink(path);
}

/*
	End of unittest of disk_cleardelete()
 */

/*
	Unittest of disk_checkdelete()
 */

TEST(disk_checkdeleteTest, Dir_markdelete_CannotAccess)
{
	ino_t root_inode;

	METAPATH = "/adadsadfg";
	root_inode = 556677;

	EXPECT_EQ(-ENOENT, disk_checkdelete(6, root_inode));
}

TEST(disk_checkdeleteTest, InodeExist_Return1)
{
	ino_t root_inode;

	METAPATH = "/tmp";
	root_inode = 556677;
	mkdir("/tmp/markdelete", 0700);
	mknod("/tmp/markdelete/inode6_556677", S_IFREG | 0700, 0);

	EXPECT_EQ(1, disk_checkdelete(6, root_inode));

	unlink("/tmp/markdelete/inode6_556677");
	nftw("/tmp/markdelete", do_delete, 20, FTW_DEPTH);
}

TEST(disk_checkdeleteTest, InodeNotExist_Return0)
{
	ino_t root_inode;

	METAPATH = "/tmp";
	root_inode = 556677;
	mkdir("/tmp/markdelete", 0700);

	EXPECT_EQ(0, disk_checkdelete(6, root_inode));

	nftw("/tmp/markdelete", do_delete, 20, FTW_DEPTH);
}

/*
	End of unittest of disk_checkdelete()
 */

/*
	Unittest of startup_finish_delete()
 */

class startup_finish_deleteTest : public ::testing::Test {
protected:
	int32_t num_inode;
	FILE *statfptr;
	FS_STAT_T tmp_stat;
	char statpath[100];

	void SetUp()
	{
		METAPATH = "testpatterns";
		snprintf(statpath, 100, "%s/stat%d", METAPATH, ROOT_INODE);
		statfptr = fopen(statpath, "w");
		memset(&tmp_stat, 0, sizeof(FS_STAT_T));
		tmp_stat.num_inodes = 1;

		fwrite(&tmp_stat, sizeof(FS_STAT_T), 1, statfptr);
		fsync(fileno(statfptr));

		fclose(statfptr);
	}

	void TearDown()
	{
		unlink(statpath);
		for (int32_t i = 0 ; i < num_inode ; i++) {
			char pathname[200];
			sprintf(pathname, "testpatterns/markdelete/inode%d_%d",
				i, ROOT_INODE);
			if (!access(pathname, F_OK))
				unlink(pathname);

			fetch_meta_path(pathname, i);
			if (!access(pathname, F_OK))
				unlink(pathname);
		}
		nftw("testpatterns/markdelete", do_delete, 20, FTW_DEPTH);

		if (!access(TO_DELETE_METAPATH, F_OK))
			unlink(TO_DELETE_METAPATH);
	}
};

TEST_F(startup_finish_deleteTest, Dir_markdelete_NotCreateYet)
{
	METAPATH = "/affdsfs";
	num_inode = 0;

	EXPECT_EQ(-ENOENT, startup_finish_delete());
}

TEST_F(startup_finish_deleteTest, DeleteInodeSuccess)
{
	char pathname[200];
	num_inode = 200;

	/* Mock inode file */
	METAPATH = "testpatterns";

	if (access("testpatterns/markdelete", F_OK) < 0)
		mkdir("testpatterns/markdelete", 0700);

	for (int32_t i = 0 ; i < num_inode ; i++) {
		char pathname[200];
		sprintf(pathname, "testpatterns/markdelete/inode%d_%d",
			i, ROOT_INODE);
		mknod(pathname, S_IFREG | 0700, 0);

		fetch_meta_path(pathname, i);
		mknod(pathname, S_IFREG, 0); // This mock metafile which will be renamed later
	}

	/* Run */
	EXPECT_EQ(0, startup_finish_delete());

	/* Verify */
	for (int32_t i = 0 ; i < num_inode ; i++) {
		char pathname[200];
		sprintf(pathname, "testpatterns/markdelete/inode%d_%d",
			i, ROOT_INODE);
		ASSERT_EQ(-1, access(pathname, F_OK));
	}
	nftw("testpatterns/markdelete", do_delete, 20, FTW_DEPTH);
}

/*
	End of unittest of startup_finish_delete()
 */

/* Unittest for change_pin_flag() */
class change_pin_flagTest : public ::testing::Test {
protected:
	void SetUp()
	{
		FILE *fptr;
		HCFS_STAT tmpstat;
		FILE_META_TYPE tmpmeta;
		FILE_STATS_TYPE tmpstats;

		fptr = fopen("test_meta_file", "wb+");
		fwrite(&tmpstat, sizeof(HCFS_STAT), 1, fptr);
		fwrite(&tmpmeta, sizeof(FILE_META_TYPE), 1, fptr);
		fwrite(&tmpstats, sizeof(FILE_STATS_TYPE), 1, fptr);
		if(fptr)
			fclose(fptr);
		test_change_pin_flag = TRUE;
		sem_init(&(hcfs_system->access_sem), 0, 1);
		hcfs_system->systemdata.system_size = MOCK_SYSTEM_SIZE;
		hcfs_system->systemdata.cache_size = MOCK_CACHE_SIZE;
		hcfs_system->systemdata.cache_blocks = MOCK_CACHE_BLOCKS;
	}

	void TearDown()
	{
		unlink("test_meta_file");
		test_change_pin_flag = FALSE;
	}
};

TEST_F(change_pin_flagTest, MetaCacheLockFail)
{
	ino_t inode = INO_LOOKUP_FILE_DATA_OK_LOCK_ENTRY_FAIL;

	EXPECT_EQ(-ENOMEM, change_pin_flag(inode, S_IFREG, TRUE));
}

TEST_F(change_pin_flagTest, RegfileHadBeenPinned)
{
	ino_t inode = INO_DIRECT_SUCCESS;

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = TRUE;
	EXPECT_EQ(1, change_pin_flag(inode, S_IFREG, TRUE));
}

TEST_F(change_pin_flagTest, PinRegfileSuccess)
{
	ino_t inode = INO_DIRECT_SUCCESS;

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = FALSE;
	EXPECT_EQ(0, change_pin_flag(inode, S_IFREG, TRUE));
}

TEST_F(change_pin_flagTest, PinFIFOfileSuccess)
{
	ino_t inode = INO_DIRECT_SUCCESS;

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = FALSE;
	EXPECT_EQ(0, change_pin_flag(inode, S_IFIFO, TRUE));
}

TEST_F(change_pin_flagTest, PinSocketfileSuccess)
{
	ino_t inode = INO_DIRECT_SUCCESS;

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = FALSE;
	EXPECT_EQ(0, change_pin_flag(inode, S_IFSOCK, TRUE));
}

TEST_F(change_pin_flagTest, DirHadBeenPinned)
{
	ino_t inode = 5452345; /* arbitrary inode */

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = TRUE;
	EXPECT_EQ(1, change_pin_flag(inode, S_IFDIR, TRUE));
}

TEST_F(change_pin_flagTest, PinDirSuccess)
{
	ino_t inode = 3142334; /* arbitrary inode */

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = FALSE;
	EXPECT_EQ(0, change_pin_flag(inode, S_IFDIR, TRUE));
}

TEST_F(change_pin_flagTest, LinkHadBeenPinned)
{
	ino_t inode = 5452345; /* arbitrary inode */

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = TRUE;
	EXPECT_EQ(1, change_pin_flag(inode, S_IFLNK, TRUE));
}

TEST_F(change_pin_flagTest, PinLinkSuccess)
{
	ino_t inode = 3142334; /* arbitrary inode */

	/* set pin flag in meta when calling meta_cache_lookup_xxx */
	pin_flag_in_meta = FALSE;
	EXPECT_EQ(0, change_pin_flag(inode, S_IFLNK, TRUE));
}

/* End of unittest for change_pin_flag() */

/* Unittest for collect_dir_children */
class collect_dir_childrenTest : public ::testing::Test {
protected:
	int64_t num_dir_node, num_nondir_node;
	ino_t *dir_node_list, *nondir_node_list;
	char *nondir_type_list;

	void SetUp()
	{
		if (!access(MOCK_META_PATH, F_OK))
			nftw(MOCK_META_PATH, do_delete, 20, FTW_DEPTH);
		mkdir(MOCK_META_PATH, 0700);
	}

	void TearDown()
	{
		if (!access(MOCK_META_PATH, F_OK))
			nftw(MOCK_META_PATH, do_delete, 20, FTW_DEPTH);
	}
};

TEST_F(collect_dir_childrenTest, MetaNotExist)
{
	ino_t inode;

	inode = 5;
	EXPECT_EQ(-ENOENT, collect_dir_children(inode, &dir_node_list,
		&num_dir_node, &nondir_node_list, &num_nondir_node,
	        &nondir_type_list));
}

TEST_F(collect_dir_childrenTest, NoChildren)
{
	ino_t inode;
	char metapath[300];
	FILE *fptr;
	HCFS_STAT tempstat;
	DIR_META_TYPE dirmeta;

	inode = 5;
	fetch_meta_path(metapath, inode);
	fptr = fopen(metapath, "w+");
	memset(&tempstat, 0, sizeof(HCFS_STAT));
	memset(&dirmeta, 0, sizeof(DIR_META_TYPE));
	fwrite(&tempstat, 1, sizeof(HCFS_STAT), fptr);
	fwrite(&dirmeta, 1, sizeof(DIR_META_TYPE), fptr);
	fclose(fptr);

	EXPECT_EQ(0, collect_dir_children(inode, &dir_node_list,
		&num_dir_node, &nondir_node_list, &num_nondir_node,
		&nondir_type_list));

	/* Verify */
	EXPECT_EQ(0, num_dir_node);
	EXPECT_EQ(0, num_nondir_node);
	EXPECT_EQ(NULL, dir_node_list);
	EXPECT_EQ(NULL, nondir_node_list);
	EXPECT_EQ(NULL, nondir_type_list);

	unlink(metapath);
}

TEST_F(collect_dir_childrenTest, CollectManyChildrenSuccess)
{
	ino_t inode, child_inode;
	char metapath[300];
	FILE *fptr;
	HCFS_STAT tempstat;
	DIR_META_TYPE dirmeta;
	DIR_ENTRY_PAGE temppage;
	memset(&temppage, 0, sizeof(DIR_ENTRY_PAGE));

	inode = 5;
	fetch_meta_path(metapath, inode);
	fptr = fopen(metapath, "w+");
	memset(&tempstat, 0, sizeof(HCFS_STAT));
	memset(&dirmeta, 0, sizeof(DIR_META_TYPE));
	dirmeta.total_children = 2;
	dirmeta.tree_walk_list_head = sizeof(HCFS_STAT) +
				sizeof(DIR_META_TYPE);
	fwrite(&tempstat, 1, sizeof(HCFS_STAT), fptr);
	fwrite(&dirmeta, 1, sizeof(DIR_META_TYPE), fptr);

	/* First page */
	temppage.num_entries = 10;
	child_inode = 1;
	for (int32_t i = 0; i < 10; i++, child_inode++) {
		strcpy(temppage.dir_entries[i].d_name, "aaa");
		temppage.dir_entries[i].d_ino = child_inode;
		temppage.dir_entries[i].d_type = child_inode % 2 ?
						D_ISDIR : D_ISREG;
	}
	temppage.tree_walk_next = sizeof(HCFS_STAT) +
		sizeof(DIR_META_TYPE) + sizeof(DIR_ENTRY_PAGE);
	fwrite(&temppage, 1, sizeof(DIR_ENTRY_PAGE), fptr);

	/* Second page */
	temppage.num_entries = 10;
	for (int32_t i = 0; i < 10; i++, child_inode++) {
		strcpy(temppage.dir_entries[i].d_name, "aaa");
		temppage.dir_entries[i].d_ino = child_inode;
		temppage.dir_entries[i].d_type = child_inode % 2 ?
						D_ISDIR : D_ISREG;
	}
	temppage.tree_walk_next = 0;
	fwrite(&temppage, 1, sizeof(DIR_ENTRY_PAGE), fptr);

	fclose(fptr);

	/* Run */
	EXPECT_EQ(0, collect_dir_children(inode, &dir_node_list,
		&num_dir_node, &nondir_node_list, &num_nondir_node,
		&nondir_type_list));

	/* Verify */
	EXPECT_EQ(10, num_dir_node);
	EXPECT_EQ(10, num_nondir_node);

	child_inode = 1;
	for (int32_t i = 0; i < num_dir_node; i++, child_inode += 2)
		EXPECT_EQ(child_inode, dir_node_list[i]);

	child_inode = 2;
	for (int32_t i = 0; i < num_nondir_node; i++, child_inode += 2) {
		EXPECT_EQ(child_inode, nondir_node_list[i]);
		EXPECT_EQ(D_ISREG, nondir_type_list[i]);
	}

	unlink(metapath);

	free(dir_node_list);
	free(nondir_node_list);
	free(nondir_type_list);
}

/* End of unittest for collect_dir_children */

/*
 * Unittest of inherit_xattr()
 */
class inherit_xattrTest : public ::testing::Test {
protected:

	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(inherit_xattrTest, XattrpageNotExist)
{
	ino_t parent_inode, this_inode;

	parent_inode = INO_NO_XATTR_PAGE;
	this_inode = 1234;

	EXPECT_EQ(0, inherit_xattr(parent_inode, this_inode, NULL));
}

TEST_F(inherit_xattrTest, TotalKeySize_Is_Zero)
{
	ino_t parent_inode, this_inode;

	parent_inode = INO_XATTR_PAGE_EXIST;
	this_inode = 1234;

	TOTAL_KEY_SIZE = 0;
	EXPECT_EQ(0, inherit_xattr(parent_inode, this_inode, NULL));
}

TEST_F(inherit_xattrTest, InsertSuccess_ValueIsLarge)
{
	ino_t parent_inode, this_inode;

	parent_inode = INO_XATTR_PAGE_EXIST;
	this_inode = 1234;

	TOTAL_KEY_SIZE = 100;
	XATTR_VALUE_SIZE = MAX_VALUE_BLOCK_SIZE * 3;
	xattr_count = 0;
	global_mock_namespace = SECURITY; /* namespace passed */
	EXPECT_EQ(0, inherit_xattr(parent_inode, this_inode, NULL));

	/* Verify those keys recorded in mock insert_xattr() */
	EXPECT_EQ(3, xattr_count);
	EXPECT_STREQ("key1", xattr_key[0]);
	EXPECT_STREQ("key2", xattr_key[1]);
	EXPECT_STREQ("key3", xattr_key[2]);
}

TEST_F(inherit_xattrTest, InsertSuccess_ValueIsSmall)
{
	ino_t parent_inode, this_inode;

	parent_inode = INO_XATTR_PAGE_EXIST;
	this_inode = 1234;

	TOTAL_KEY_SIZE = 100;
	XATTR_VALUE_SIZE = MAX_VALUE_BLOCK_SIZE / 2;
	xattr_count = 0;
	global_mock_namespace = SECURITY; /* namespace passed */
	EXPECT_EQ(0, inherit_xattr(parent_inode, this_inode, NULL));

	/* Verify those keys recorded in mock insert_xattr() */
	EXPECT_EQ(3, xattr_count);
	EXPECT_STREQ("key1", xattr_key[0]);
	EXPECT_STREQ("key2", xattr_key[1]);
	EXPECT_STREQ("key3", xattr_key[2]);
}

TEST_F(inherit_xattrTest, NameSpace_USER_NOT_Pass)
{
	ino_t parent_inode, this_inode;

	parent_inode = INO_XATTR_PAGE_EXIST;
	this_inode = 1234;

	TOTAL_KEY_SIZE = 100;
	XATTR_VALUE_SIZE = MAX_VALUE_BLOCK_SIZE / 2;
	xattr_count = 0;
	global_mock_namespace = USER; /* namespace NOT passed */
	EXPECT_EQ(0, inherit_xattr(parent_inode, this_inode, NULL));

	/* Verify those keys recorded in mock insert_xattr() */
	EXPECT_EQ(0, xattr_count);
}

TEST_F(inherit_xattrTest, NameSpace_SYSTEM_NOT_Pass)
{
	ino_t parent_inode, this_inode;

	parent_inode = INO_XATTR_PAGE_EXIST;
	this_inode = 1234;

	TOTAL_KEY_SIZE = 100;
	XATTR_VALUE_SIZE = MAX_VALUE_BLOCK_SIZE / 2;
	xattr_count = 0;
	global_mock_namespace = SYSTEM; /* namespace NOT passed */
	EXPECT_EQ(0, inherit_xattr(parent_inode, this_inode, NULL));

	/* Verify those keys recorded in mock insert_xattr() */
	EXPECT_EQ(0, xattr_count);
}
/*
 * End of unittest of inherit_xattr()
 */

/**
 * Unittest of restore_meta_file()
 */
class restore_meta_fileTest : public ::testing::Test {
protected:
	char work_path[300];

	void SetUp()
	{
		strcpy(work_path, "restore_meta_fileTestPath");
		if (!access(work_path, F_OK))
			system("rm -rf ./restore_meta_fileTestPath");
		mkdir(work_path, 0700);
		hcfs_system->backend_is_online = TRUE;
		hcfs_system = (SYSTEM_DATA_HEAD*)
				malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		free(hcfs_system);
		if (!access(work_path, F_OK))
			system("rm -rf ./restore_meta_fileTestPath");
	}
};

TEST_F(restore_meta_fileTest, MetaHadBeenRestored)
{
	ino_t inode;
	char mock_metapath[300];

	inode = 5;
	fetch_meta_path(mock_metapath, inode);
	mknod(mock_metapath, 0600, 0);

	EXPECT_EQ(0, restore_meta_file(inode));
	EXPECT_EQ(0, access(mock_metapath, F_OK));
	unlink(mock_metapath);
}

TEST_F(restore_meta_fileTest, NoConnection)
{
	ino_t inode;
	char mock_metapath[300];

	inode = 5;
	hcfs_system->backend_is_online = FALSE;

	EXPECT_EQ(-ENOTCONN, restore_meta_file(inode));
}

TEST_F(restore_meta_fileTest, RestoreDir_Success)
{
	ino_t inode;
	char mock_metapath[400], mock_restore_path[400];
	FILE *fptr;
	struct stat tmpstat;
	DIR_META_TYPE dirmeta;
	CLOUD_RELATED_DATA cloud_data, exp_cloud_data;
	int64_t meta_size;

	/* Generate mock restored file */
	inode = 5;
	fetch_restored_meta_path(mock_restore_path, inode);
	fptr = fopen(mock_restore_path, "w+");
	setbuf(fptr, NULL);
	memset(&tmpstat, 0, sizeof(struct stat));
	memset(&dirmeta, 0, sizeof(DIR_META_TYPE));
	memset(&cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
	tmpstat.st_mode = S_IFDIR;

	fwrite(&tmpstat, sizeof(struct stat), 1, fptr);
	fwrite(&dirmeta, sizeof(DIR_META_TYPE), 1, fptr);
	fwrite(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	fstat(fileno(fptr), &tmpstat);
	fclose(fptr);
	
	memset(&exp_cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
	exp_cloud_data.size_last_upload = tmpstat.st_size;
	exp_cloud_data.meta_last_upload = tmpstat.st_size;
	exp_cloud_data.upload_seq = 1;

	hcfs_system->backend_is_online = TRUE;
	
	/* Run */
	EXPECT_EQ(0, restore_meta_file(inode));

	/* Verify */
	fetch_meta_path(mock_metapath, inode);
	ASSERT_EQ(0, access(mock_metapath, F_OK));
	fptr = fopen(mock_metapath, "r");
	fseek(fptr, sizeof(struct stat) + sizeof(DIR_META_TYPE), SEEK_SET);
	fread(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	fclose(fptr);
	EXPECT_EQ(0, memcmp(&exp_cloud_data, &cloud_data,
			sizeof(CLOUD_RELATED_DATA)));
	unlink(mock_metapath);

	EXPECT_EQ(tmpstat.st_size, hcfs_system->systemdata.system_size);
}

TEST_F(restore_meta_fileTest, RestoreSymlink_Success)
{
	ino_t inode;
	char mock_metapath[400], mock_restore_path[400];
	FILE *fptr;
	struct stat tmpstat;
	SYMLINK_META_TYPE symmeta;
	CLOUD_RELATED_DATA cloud_data, exp_cloud_data;
	int64_t meta_size;

	/* Generate mock restored file */
	inode = 5;
	fetch_restored_meta_path(mock_restore_path, inode);
	fptr = fopen(mock_restore_path, "w+");
	setbuf(fptr, NULL);
	memset(&tmpstat, 0, sizeof(struct stat));
	memset(&symmeta, 0, sizeof(SYMLINK_META_TYPE));
	memset(&cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
	tmpstat.st_mode = S_IFLNK;

	fwrite(&tmpstat, sizeof(struct stat), 1, fptr);
	fwrite(&symmeta, sizeof(SYMLINK_META_TYPE), 1, fptr);
	fwrite(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	fstat(fileno(fptr), &tmpstat);
	fclose(fptr);
	
	memset(&exp_cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
	exp_cloud_data.size_last_upload = tmpstat.st_size;
	exp_cloud_data.meta_last_upload = tmpstat.st_size;
	exp_cloud_data.upload_seq = 1;

	hcfs_system->backend_is_online = TRUE;
	
	/* Run */
	EXPECT_EQ(0, restore_meta_file(inode));

	/* Verify */
	fetch_meta_path(mock_metapath, inode);
	ASSERT_EQ(0, access(mock_metapath, F_OK));
	fptr = fopen(mock_metapath, "r");
	fseek(fptr, sizeof(struct stat) + sizeof(SYMLINK_META_TYPE), SEEK_SET);
	fread(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	fclose(fptr);
	EXPECT_EQ(0, memcmp(&exp_cloud_data, &cloud_data,
			sizeof(CLOUD_RELATED_DATA)));
	unlink(mock_metapath);

	EXPECT_EQ(tmpstat.st_size, hcfs_system->systemdata.system_size);
}

TEST_F(restore_meta_fileTest, RestoreRegfile_Success)
{
	ino_t inode;
	char mock_metapath[400], mock_restore_path[400];
	FILE *fptr;
	struct stat tmpstat;
	FILE_META_TYPE filemeta;
	FILE_STATS_TYPE stats, verified_stats, exp_stats;
	CLOUD_RELATED_DATA cloud_data, exp_cloud_data;
	BLOCK_ENTRY_PAGE block_page, verified_block_page;
	int64_t meta_size;
	char status_list[7] = {ST_NONE, ST_TODELETE, ST_LDISK,
			ST_LtoC, ST_CLOUD, ST_CtoL, ST_BOTH};
	int seq_list[7] = {123, 234, 345, 456, 567, 678, 789};


	/* Generate mock restored file */
	inode = 5;
	fetch_restored_meta_path(mock_restore_path, inode);
	fptr = fopen(mock_restore_path, "w+");
	setbuf(fptr, NULL);
	memset(&tmpstat, 0, sizeof(struct stat));
	memset(&filemeta, 0, sizeof(FILE_META_TYPE));
	memset(&stats, 1, sizeof(FILE_STATS_TYPE));
	stats.num_blocks = 7;
	memset(&cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
	memset(&block_page, 0, sizeof(BLOCK_ENTRY_PAGE));

	tmpstat.st_mode = S_IFREG;
	tmpstat.st_size = MAX_BLOCK_SIZE * 7;
	filemeta.direct = sizeof(struct stat) + sizeof(FILE_META_TYPE) +
			sizeof(FILE_STATS_TYPE) + sizeof(CLOUD_RELATED_DATA);
	filemeta.local_pin = P_HIGH_PRI_PIN;
	block_page.num_entries = 7;
	for (int i = 0; i < 7; i++) {
		block_page.block_entries[i].status = status_list[i];
		block_page.block_entries[i].seqnum = seq_list[i];
	}

	fwrite(&tmpstat, sizeof(struct stat), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	fwrite(&stats, sizeof(FILE_STATS_TYPE), 1, fptr);
	fwrite(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	fwrite(&block_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fstat(fileno(fptr), &tmpstat);
	fclose(fptr);

	/* Expected result */
	memset(&exp_stats, 0, sizeof(FILE_STATS_TYPE));
	exp_stats.num_blocks = 7;
	memset(&exp_cloud_data, 0, sizeof(CLOUD_RELATED_DATA));
	exp_cloud_data.size_last_upload = MAX_BLOCK_SIZE * 7 + tmpstat.st_size;
	exp_cloud_data.meta_last_upload = tmpstat.st_size;
	exp_cloud_data.upload_seq = 1;

	hcfs_system->backend_is_online = TRUE;
	
	/* Run */
	EXPECT_EQ(0, restore_meta_file(inode));

	/* Verify */
	fetch_meta_path(mock_metapath, inode);
	ASSERT_EQ(0, access(mock_metapath, F_OK));
	fptr = fopen(mock_metapath, "r");
	fseek(fptr, sizeof(struct stat) + sizeof(FILE_META_TYPE), SEEK_SET);
	fread(&verified_stats, sizeof(FILE_STATS_TYPE), 1, fptr);
	fread(&cloud_data, sizeof(CLOUD_RELATED_DATA), 1, fptr);
	fread(&verified_block_page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);
	fclose(fptr);
	for (int i = 0; i < 7; i++) {
		ASSERT_EQ(seq_list[i],
			verified_block_page.block_entries[i].seqnum);
		if (status_list[i] == ST_NONE ||
				status_list[i] == ST_TODELETE)
			ASSERT_EQ(ST_NONE,
				verified_block_page.block_entries[i].status);
		else
			ASSERT_EQ(ST_CLOUD,
				verified_block_page.block_entries[i].status);
	}

	EXPECT_EQ(0, memcmp(&exp_stats, &verified_stats,
			sizeof(FILE_STATS_TYPE)));
	EXPECT_EQ(0, memcmp(&exp_cloud_data, &cloud_data,
			sizeof(CLOUD_RELATED_DATA)));
	unlink(mock_metapath);

	EXPECT_EQ(tmpstat.st_size + MAX_BLOCK_SIZE * 7,
			hcfs_system->systemdata.system_size);
}

/**
 * End unittest of restore_meta_file()
 */

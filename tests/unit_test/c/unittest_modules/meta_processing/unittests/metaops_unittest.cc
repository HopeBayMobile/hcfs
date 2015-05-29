#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "mock_param.h"

#include "utils.h"
#include "global.h"
#include "hfuse_system.h"
#include "fuseop.h"
#include "params.h"
#include "metaops.h"
}
#include "gtest/gtest.h"

/* Global vars  */
static const ino_t self_inode = 10;
static const ino_t parent_inode = 5;

extern int DELETE_DIR_ENTRY_BTREE_RESULT;
extern SYSTEM_CONF_STRUCT system_config;

/*
	Unittest of init_dir_page()
 */
TEST(init_dir_pageTest, InitOK) 
{
        long long pos = 1000;

	DIR_ENTRY_PAGE *temppage = (DIR_ENTRY_PAGE*)malloc(sizeof(DIR_ENTRY_PAGE));
	
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
                       	strcpy(mock_metaname, "/tmp/mock_meta_used_in_dir_add_entry"); 
			self_name = "selfname";
			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(
				sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
			body_ptr->fptr = fopen(mock_metaname, "w+");
			/* Init meta & stat to be verified */
			memset(&to_verified_meta, 0, sizeof(FILE_META_TYPE));
			memset(&to_verified_stat, 0, sizeof(struct stat));
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
		self_name, S_IFMT, body_ptr));
}

TEST_F(dir_add_entryTest, insert_dir_entryFail)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;
	fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	sem_wait(&body_ptr->access_sem);
	
	/* Run */
	EXPECT_EQ(-1, dir_add_entry(parent_inode, INO_INSERT_DIR_ENTRY_FAIL,
		self_name, S_IFMT, body_ptr));
}

TEST_F(dir_add_entryTest, AddRegFileSuccess_WithoutSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;
	fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	to_verified_meta.total_children = TOTAL_CHILDREN_NUM; 
	to_verified_stat.st_nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);
	
	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITHOUT_SPLITTING, 
		self_name, S_IFREG, body_ptr));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.st_nlink);
}

TEST_F(dir_add_entryTest, AddDirSuccess_WithoutSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;
	fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	to_verified_meta.total_children = TOTAL_CHILDREN_NUM; 
	to_verified_stat.st_nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);
	
	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITHOUT_SPLITTING, 
		self_name, S_IFDIR, body_ptr));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM + 1, to_verified_stat.st_nlink);
}

TEST_F(dir_add_entryTest, AddRegFileSuccess_WithSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;

	for (int i = 0 ; i < 5 ; i++) // 5 pages
		fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	to_verified_meta.total_children = TOTAL_CHILDREN_NUM; 
	to_verified_stat.st_nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);
	
	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITH_SPLITTING, 
		self_name, S_IFREG, body_ptr));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.st_nlink);
}

TEST_F(dir_add_entryTest, AddDirSuccess_WithSplittingRoot)
{
	/* Mock data */
	DIR_ENTRY_PAGE tmp_dir_page;

	for (int i = 0 ; i < 5 ; i++) // 5 pages
		fwrite(&tmp_dir_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	to_verified_meta.total_children = TOTAL_CHILDREN_NUM; 
	to_verified_stat.st_nlink = LINK_NUM;
	sem_wait(&body_ptr->access_sem);
	
	/* Run */
	EXPECT_EQ(0, dir_add_entry(parent_inode, 
		INO_INSERT_DIR_ENTRY_SUCCESS_WITH_SPLITTING, 
		self_name, S_IFDIR, body_ptr));
	EXPECT_EQ(TOTAL_CHILDREN_NUM + 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM + 1, to_verified_stat.st_nlink);
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

			testpage = (DIR_ENTRY_PAGE*)malloc(sizeof(DIR_ENTRY_PAGE));
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
			memset(&to_verified_stat, 0, sizeof(struct stat));
			to_verified_meta.total_children = TOTAL_CHILDREN_NUM; 
			to_verified_stat.st_nlink = LINK_NUM;
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
	EXPECT_EQ(-1, dir_remove_entry(parent_inode, self_inode, self_name, S_IFMT, body_ptr));
}

TEST_F(dir_remove_entryTest, BtreeDelFailed_RemoveEntryFail) 
{
	/* Mock data to force btree deletion failed */
	sem_wait(&(body_ptr->access_sem));
	DELETE_DIR_ENTRY_BTREE_RESULT = 0;
	
	/* Run tested function */
	EXPECT_EQ(-1, dir_remove_entry(parent_inode, self_inode, self_name, S_IFMT, body_ptr));
		
	/* Verify */
	EXPECT_EQ(TOTAL_CHILDREN_NUM, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.st_nlink);
	sem_post(&(body_ptr->access_sem));
}

TEST_F(dir_remove_entryTest, RemoveDirSuccess) 
{
	/* Mock data */
	sem_wait(&(body_ptr->access_sem));
	DELETE_DIR_ENTRY_BTREE_RESULT = 1;
	
	/* Run tested function */
	EXPECT_EQ(0, dir_remove_entry(parent_inode, self_inode, self_name, S_IFDIR, body_ptr));

	/* Verify */
	EXPECT_EQ(TOTAL_CHILDREN_NUM - 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM - 1, to_verified_stat.st_nlink);
	sem_post(&(body_ptr->access_sem));
}

TEST_F(dir_remove_entryTest, RemoveRegFileSuccess) 
{
	/* Mock data */
	sem_wait(&(body_ptr->access_sem));
	DELETE_DIR_ENTRY_BTREE_RESULT = 1;
	
	/* Run tested function */
	EXPECT_EQ(0, dir_remove_entry(parent_inode, self_inode, self_name, S_IFREG, body_ptr));

	/* Verify */
	EXPECT_EQ(TOTAL_CHILDREN_NUM - 1, to_verified_meta.total_children);
	EXPECT_EQ(LINK_NUM, to_verified_stat.st_nlink);
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
	EXPECT_EQ(0, change_parent_inode(INO_SEEK_DIR_ENTRY_OK, parent_inode, parent_inode2, body_ptr));
}

TEST_F(change_parent_inodeTest, DirEntryNotFound) 
{
	EXPECT_EQ(-1, change_parent_inode(INO_SEEK_DIR_ENTRY_NOTFOUND, parent_inode, parent_inode2, body_ptr));
}

TEST_F(change_parent_inodeTest, ChangeFail) 
{
	EXPECT_EQ(-1, change_parent_inode(INO_SEEK_DIR_ENTRY_FAIL, parent_inode, parent_inode2, body_ptr));
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

		body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		memset(&to_verified_meta, 0, sizeof(FILE_META_TYPE));
		memset(&to_verified_stat, 0, sizeof(struct stat));
	}
	virtual void TearDown() {
		free(body_ptr);
	}
};

TEST_F(change_dir_entry_inodeTest, ChangeOK) 
{	
	EXPECT_EQ(0, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_OK, "/mock/target/name", new_inode, body_ptr));
}

TEST_F(change_dir_entry_inodeTest, DirEntryNotFound) 
{
	EXPECT_EQ(-1, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_NOTFOUND, "/mock/target/name", new_inode, body_ptr));
}

TEST_F(change_dir_entry_inodeTest, ChangeFail) 
{
	EXPECT_EQ(-1, change_dir_entry_inode(INO_SEEK_DIR_ENTRY_FAIL, "/mock/target/name", new_inode, body_ptr));
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
		FILE *tarfptr = fopen(META_PATH, "w+");
		FILE *srcfptr = fopen("testpatterns/mock_meta_file", "r");
		
		cp_file(srcfptr, tarfptr);
		fclose(srcfptr);
		fclose(tarfptr);
	}
	void TearDown()
	{
		unlink(META_PATH);
		unlink(TO_DELETE_METAPATH);
	}
	/* This function is used to check rename or move success  */
	bool is_file_diff(FILE *f1, FILE *f2)
	{
		bool is_diff = false;
		while (!feof(f1) || !feof(f2)) {
			char buf1[5000], buf2[5000];
			int read_size1 = fread(buf1, 1, 4096, f1);
			int read_size2 = fread(buf2, 1, 4096, f2);
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
	void cp_file(FILE *src, FILE *tar)
	{
		char filebuf[5000];

		fseek(src, 0, SEEK_SET);
		fseek(tar, 0, SEEK_SET);
		while (!feof(src)) {
			int read_size = fread(filebuf, 1, 4096, src);
			if (read_size > 0)
				fwrite(filebuf, 1, read_size, tar);
			else
				break;
		}

	}
};
TEST_F(delete_inode_metaTest, DirectlyRenameSuccess)
{
	FILE *todeletefptr, *expectedfptr;
	ino_t inode = INO_RENAME_SUCCESS;

	/* Run */
	EXPECT_EQ(0, delete_inode_meta(inode));

	/* Verify */
	EXPECT_EQ(0, access(TO_DELETE_METAPATH, F_OK));
	EXPECT_EQ(-1, access(META_PATH, F_OK));

	todeletefptr = fopen(TO_DELETE_METAPATH, "r");
	expectedfptr = fopen("testpatterns/mock_meta_file", "r");
	
	ASSERT_TRUE(todeletefptr != NULL);
	ASSERT_TRUE(expectedfptr != NULL);
	EXPECT_FALSE(is_file_diff(todeletefptr, expectedfptr));

	/* Free resource */
	fclose(todeletefptr);
	fclose(expectedfptr);
}

TEST_F(delete_inode_metaTest, RenameFail)
{
	//FILE *todeletefptr, *expectedfptr;
	ino_t inode = INO_RENAME_FAIL;

	/* Run */
	EXPECT_EQ(-1, delete_inode_meta(inode));
}


/*
	End of unittest of delete_inode_meta()
 */

/*
	Unittest of decrease_nlink_inode_file()
 */
class decrease_nlink_inode_fileTest : public ::testing::Test {
protected:
	virtual void SetUp() 
	{
		/* Mock user-defined parameters */
		MAX_BLOCK_SIZE = PARAM_MAX_BLOCK_SIZE;

		/* Mock system statistics */
		hcfs_system = (SYSTEM_DATA_HEAD*)malloc(sizeof(SYSTEM_DATA_HEAD));
		sem_init(&(hcfs_system->access_sem), 0, 1);
		hcfs_system->systemdata.system_size = MOCK_SYSTEM_SIZE;
		hcfs_system->systemdata.cache_size = MOCK_CACHE_SIZE;
		hcfs_system->systemdata.cache_blocks = MOCK_CACHE_BLOCKS;
	}

	virtual void TearDown() 
	{
		free(hcfs_system);
	}
};

TEST_F(decrease_nlink_inode_fileTest, InodeStillReferenced) 
{
	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_STLINK_2));
}

TEST_F(decrease_nlink_inode_fileTest, NoBlockFilesToDel) 
{
	char metapath[METAPATHLEN];
	char thisblockpath[400];
	FILE *tmp_file;

	fetch_meta_path(metapath, INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel);
	tmp_file = fopen(metapath, "w");
	fclose(tmp_file);
	
	/* Test */
	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel));
	
	/* Verify */
	EXPECT_EQ(MOCK_SYSTEM_SIZE, hcfs_system->systemdata.system_size);

	unlink(metapath);
}

TEST_F(decrease_nlink_inode_fileTest, BlockFilesToDel) 
{
	char metapath[METAPATHLEN];
	char thisblockpath[400];
	bool block_file_existed = false;
	FILE *tmp_fp;

	fetch_meta_path(metapath, INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel);
	tmp_fp = fopen(metapath, "w");
	fclose(tmp_fp);
	
	/* Test  */
	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel));
	
	unlink(metapath);
	
}
/* TODO - Test for metafile rename failed case */
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
	long long pointers_per_page[5];

	void SetUp()
	{
		FILE_META_TYPE empty_file_meta;

		metapath = "testpatterns/seek_page_meta_file";
		body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
		sem_init(&(body_ptr->access_sem), 0, 1);
		body_ptr->fptr = fopen(metapath, "w+");
		setbuf(body_ptr->fptr, NULL);
		
		fwrite(&empty_file_meta, sizeof(FILE_META_TYPE), 1, body_ptr->fptr);
		body_ptr->meta_opened = TRUE;
		
		pointers_per_page[0] = 1;
		for (int i = 1 ; i < 5 ; i++)
			pointers_per_page[i] = pointers_per_page[i - 1] * POINTERS_PER_PAGE;
	}

	void TearDown()
	{
		if (body_ptr->fptr)
			fclose(body_ptr->fptr);
		unlink(metapath);
		free(body_ptr);
	}

	void write_mock_file_meta(int deep, long long target_page, long long expected_pos)
	{
		long long tmp_target_page;
		PTR_ENTRY_PAGE ptr_entry_page;

		memset(&ptr_entry_page, 0, sizeof(PTR_ENTRY_PAGE)); // Init all entry as 0
		tmp_target_page = target_page;
		for (int level = 0 ; level < deep ; level++) 
			tmp_target_page -= pointers_per_page[level];
		for (int level = 1 ; level <= deep ; level++) { // Write level 1, 2, 3 index
			int level_index = (tmp_target_page) / pointers_per_page[deep - level];
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
	long long actual_pos;
	long long expected_pos = sizeof(FILE_META_TYPE);
	long long target_page = 0;

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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page = POINTERS_PER_PAGE / 2; // Medium of range(1, 1024)
	long long tmp_target_page = target_page - 1;
	
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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page, tmp_target_page;
	int level1_index, level2_index;

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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page;

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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page;

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
	long long pointers_per_page[5];
	META_CACHE_ENTRY_STRUCT *body_ptr;

	void SetUp()
	{
		FILE_META_TYPE empty_file_meta;

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
		for (int i = 1 ; i < 5 ; i++)
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
	bool verify_mock_file_meta(int deep, long long target_page)
	{
		long long tmp_target_page;
		PTR_ENTRY_PAGE ptr_entry_page;
		BLOCK_ENTRY_PAGE created_block_page, zero_block_page;
		int result;

		tmp_target_page = target_page;
		for (int level = 0 ; level < deep ; level++) 
			tmp_target_page -= pointers_per_page[level];
		
		fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);
		for (int level = 1 ; level <= deep ; level++) { // Write level 1, 2, 3 index
			int level_index = tmp_target_page / pointers_per_page[deep - level];
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

TEST_F(create_pageTest, DirectPageCreateSuccess)
{
	/* Mock data */
	long long target_page;
	long long result_pos;
	long long expected_pos;
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
	long long target_page;
	long long result_pos;
	long long expected_pos;
	
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
	long long target_page;
	long long result_pos;
	long long expected_pos;
	
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
	long long target_page;
	long long result_pos;
	long long expected_pos;
	
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
	long long target_page;
	long long result_pos;
	long long expected_pos;
	
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
	long long actual_pos;
	long long expected_pos = sizeof(FILE_META_TYPE);
	long long target_page = 0;
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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page = pointers_per_page[1] / 4 * 3; // In range(1, 1024)
	
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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page = (pointers_per_page[1] + 
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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page = (pointers_per_page[2] + 
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
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page = (pointers_per_page[3] + 
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

TEST(actual_delete_inodeTest, DeleteDirSuccess)
{
	EXPECT_EQ(0, actual_delete_inode(25, D_ISDIR));
}

TEST(actual_delete_inodeTest, DeleteRegFileSuccess)
{
	FILE *tmp_fp;
	char thisblockpath[100];
	bool block_file_existed;
	ino_t mock_inode = INO_DELETE_FILE_BLOCK;

	/* Mock system init data & block data */
	MAX_BLOCK_SIZE = PARAM_MAX_BLOCK_SIZE;
	hcfs_system = (SYSTEM_DATA_HEAD*)malloc(sizeof(SYSTEM_DATA_HEAD));
	sem_init(&(hcfs_system->access_sem), 0, 1);
	hcfs_system->systemdata.system_size = MOCK_SYSTEM_SIZE;
	hcfs_system->systemdata.cache_size = MOCK_CACHE_SIZE;
	hcfs_system->systemdata.cache_blocks = MOCK_CACHE_BLOCKS;

	for (int i = 0; i < NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, mock_inode, i);
		tmp_fp = fopen(thisblockpath, "w");
		fclose(tmp_fp);
	}
	
	/* Run */
	EXPECT_EQ(0, actual_delete_inode(mock_inode, D_ISREG));
	EXPECT_EQ(MOCK_SYSTEM_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS, 
		hcfs_system->systemdata.system_size);
	EXPECT_EQ(MOCK_CACHE_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS, 
		hcfs_system->systemdata.cache_size);
	EXPECT_EQ(MOCK_CACHE_BLOCKS - NUM_BLOCKS, 
		hcfs_system->systemdata.cache_blocks);

	/* Test if block files are removed correctly */
	block_file_existed = false;
	for (int i = 0; i < NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, mock_inode, i);
		tmp_fp = fopen(thisblockpath, "r");
		if (tmp_fp) {
			block_file_existed = true;
			fclose(tmp_fp);
			unlink(thisblockpath);
		}
	}
	EXPECT_EQ(false, block_file_existed);
	
	free(hcfs_system);
}

/*
	End of unittest of actual_delete_inode()
 */

/*
	Unittest of disk_markdelete()
 */

TEST(disk_markdeleteTest, MakeDir_markdelete_Fail)
{
	METAPATH = "\0";

	EXPECT_EQ(-1, disk_markdelete(6));
}

TEST(disk_markdeleteTest, MarkSuccess)
{
	METAPATH = "/tmp";
	
	/* Run */
	EXPECT_EQ(0, disk_markdelete(6));
	
	/* Verify */
	EXPECT_EQ(0, access("/tmp/markdelete", F_OK));
	EXPECT_EQ(0, access("/tmp/markdelete/inode6", F_OK));
	
	unlink("/tmp/markdelete/inode6");
	rmdir("/tmp/markdelete");
}

/*
	End of unittest of disk_markdelete()
 */

/*
	Unittest of disk_cleardelete()
 */

TEST(disk_cleardeleteTest, Dir_markdelete_NotExist)
{
	METAPATH = "\0";

	EXPECT_EQ(-1, disk_cleardelete(6));
}

TEST(disk_cleardeleteTest, InodeNotExist)
{
	METAPATH = "/tmp";
	mkdir("/tmp/markdelete", 0700);
	
	EXPECT_EQ(0, disk_cleardelete(6));

	rmdir("/tmp/markdelete");
}

TEST(disk_cleardeleteTest, ClearInodeSuccess)
{	
	METAPATH = "/tmp";
	mkdir("/tmp/markdelete", 0700);
	mknod("/tmp/markdelete/inode6", S_IFREG | 0700, 0);

	EXPECT_EQ(0, disk_cleardelete(6));

	unlink("/tmp/markdelete/inode6");
	rmdir("/tmp/markdelete");
}

/*
	End of unittest of disk_cleardelete()
 */

/*
	Unittest of disk_checkdelete()
 */

TEST(disk_checkdeleteTest, Dir_markdelete_CannotAccess)
{
	METAPATH = "\0";

	EXPECT_EQ(-ENOENT, disk_cleardelete(6));
}

TEST(disk_checkdeleteTest, InodeExist_Return1)
{
	METAPATH = "/tmp";
	mkdir("/tmp/markdelete", 0700);
	mknod("/tmp/markdelete/inode6", S_IFREG | 0700, 0);

	EXPECT_EQ(1, disk_checkdelete(6));

	unlink("/tmp/markdelete/inode6");
	rmdir("/tmp/markdelete");
}

TEST(disk_checkdeleteTest, InodeNotExist_Return0)
{
	METAPATH = "/tmp";
	mkdir("/tmp/markdelete", 0700);

	EXPECT_EQ(0, disk_checkdelete(6));

	rmdir("/tmp/markdelete");
}

/*
	End of unittest of disk_checkdelete()
 */

/*
	Unittest of startup_finish_delete()
 */

TEST(startup_finish_deleteTest, Dir_markdelete_NotCreateYet)
{
	METAPATH = "\0";

	EXPECT_EQ(-ENOENT, startup_finish_delete());
}

TEST(startup_finish_deleteTest, DeleteInodeSuccess)
{
	/* Mock inode file */
	int num_inode = 200;

	METAPATH = "/tmp";
	ASSERT_EQ(0, mkdir("/tmp/markdelete", 0700));
	for (int i = 0 ; i < num_inode ; i++) {
		char pathname[200];
		sprintf(pathname, "/tmp/markdelete/inode%d", i);
		ASSERT_EQ(0, mknod(pathname, S_IFREG | 0700, 0));
	}

	/* Run */
	EXPECT_EQ(0, startup_finish_delete());

	/* Verify */
	for (int i = 0 ; i < num_inode ; i++) {
		char pathname[200];
		sprintf(pathname, "/tmp/markdelete/inode%d", i);
		ASSERT_EQ(-1, access(pathname, F_OK));
	}
	rmdir("/tmp/markdelete");
}

/*
	End of unittest of startup_finish_delete()
 */

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
	bool cp_file(FILE *src, FILE *tar)
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
	FILE *todeletefptr, *expectedfptr;
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
	
	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_NoBlocksToDel));
	EXPECT_EQ(MOCK_SYSTEM_SIZE, hcfs_system->systemdata.system_size);
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
	
	for (int i=0; i<NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel, i);
		tmp_fp = fopen(thisblockpath, "w");
		fclose(tmp_fp);
	}

	/* Test  */
	EXPECT_EQ(0, decrease_nlink_inode_file(INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel));
	EXPECT_EQ((MOCK_SYSTEM_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS), hcfs_system->systemdata.system_size);
	EXPECT_EQ((MOCK_CACHE_SIZE - MOCK_BLOCK_SIZE*NUM_BLOCKS), hcfs_system->systemdata.cache_size);
	EXPECT_EQ((MOCK_CACHE_BLOCKS - NUM_BLOCKS), hcfs_system->systemdata.cache_blocks);

	/* Test if block files are removed correctly */
	for (int i=0; i<NUM_BLOCKS; i++) {
		fetch_block_path(thisblockpath, INO_LOOKUP_DIR_DATA_OK_WITH_BlocksToDel, i);
		tmp_fp = fopen(thisblockpath, "r");
		if (tmp_fp) {
			block_file_existed = true;
			fclose(tmp_fp);
		}
	}
	EXPECT_EQ(false, block_file_existed);
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
		for (int i = 1 ; i<5 ; i++)
			pointers_per_page[i] = pointers_per_page[i - 1] * POINTERS_PER_PAGE;
	}

	void TearDown()
	{
		if (body_ptr->fptr)
			fclose(body_ptr->fptr);
		unlink(metapath);
		free(body_ptr);
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
	
	body_ptr->inode_num = INO_SINGLE_INDIRECT_SUCCESS;
	memset(&ptr_entry_page, 0, sizeof(PTR_ENTRY_PAGE));
	ptr_entry_page.ptr[target_page - 1] = expected_pos; // Set expected result
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

	target_page = POINTERS_PER_PAGE * 2;
	//	(POINTERS_PER_PAGE + 1); // Medium of range(1024, 1024^2)
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
	PTR_ENTRY_PAGE ptr_entry_page;
	long long actual_pos;
	long long expected_pos = 5566;
	long long target_page, tmp_target_page;
	int level1_index, level2_index, level3_index;

	target_page = POINTERS_PER_PAGE / 2 * POINTERS_PER_PAGE * 
		(POINTERS_PER_PAGE + 1); // Medium of range(1024^2, 1024^3)
	body_ptr->inode_num = INO_TRIPLE_INDIRECT_SUCCESS;
	memset(&ptr_entry_page, 0, sizeof(PTR_ENTRY_PAGE));
	fseek(body_ptr->fptr, sizeof(FILE_META_TYPE), SEEK_SET);

	tmp_target_page = target_page -
		 pointers_per_page[0] - 
		 pointers_per_page[1] - 
		 pointers_per_page[2];
	for (int level = 1 ; level <= 3 ; level++) { // Write level 1, 2, 3 index
		int level_index = (tmp_target_page) / pointers_per_page[3 - level];
		ptr_entry_page.ptr[level_index] = (level == 3 ? expected_pos :
			sizeof(FILE_META_TYPE) + sizeof(PTR_ENTRY_PAGE) * level);
		fwrite(&ptr_entry_page, sizeof(PTR_ENTRY_PAGE), 1, body_ptr->fptr);
		ptr_entry_page.ptr[level_index] = 0;
		tmp_target_page = (tmp_target_page) % pointers_per_page[3 - level];
	}

	/* Run */
	sem_wait(&body_ptr->access_sem);
	actual_pos = seek_page(body_ptr, target_page, 0);
	
	/* Verify */
	EXPECT_EQ(expected_pos, actual_pos);

}


 /*
class seek_pageTest : public ::testing::Test {
	protected:

		char *metapath;
		long long target_page;

		FH_ENTRY *fh_ptr;
		META_CACHE_ENTRY_STRUCT *body_ptr;	

		virtual void SetUp() {
			metapath = "testpatterns/seek_page_meta_file";

			target_page = 0;

			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
			body_ptr->fptr = fopen(metapath, "w+");
			setbuf(body_ptr->fptr, NULL);
			body_ptr->meta_opened = TRUE;

			fh_ptr = (FH_ENTRY*)malloc(sizeof(FH_ENTRY));
			fh_ptr->thisinode = self_inode;
			fh_ptr->meta_cache_ptr = body_ptr;
		}

                virtual void TearDown() {
			fclose(body_ptr->fptr);
			free(fh_ptr);
			free(body_ptr);
			remove(metapath);
		}
};
TEST_F(seek_pageTest, NoLockError) {
	EXPECT_EQ(-1, seek_page(fh_ptr, target_page));
}
TEST_F(seek_pageTest, LookupFileDataFailed) {
	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_FAIL;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(-1, seek_page(fh_ptr, target_page));
	sem_post(&(body_ptr->access_sem));
}
TEST_F(seek_pageTest, UpdateFileDataFailed) {
	fh_ptr->thisinode = INO_UPDATE_FILE_DATA_FAIL;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(-1, seek_page(fh_ptr, target_page));
	sem_post(&(body_ptr->access_sem));
}
TEST_F(seek_pageTest, TargetPageExisted) {
	target_page = NUM_BLOCKS - 1;

	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_OK;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(0, seek_page(fh_ptr, target_page));
	EXPECT_EQ(target_page, fh_ptr->cached_page_index);
	// Assume the position of first page is 0 
	EXPECT_EQ((target_page+1)*sizeof(BLOCK_ENTRY_PAGE), fh_ptr->cached_filepos);
	sem_post(&(body_ptr->access_sem));
}
// Test for target block page isn't generated 
TEST_F(seek_pageTest, TargetPageNotExisted) {
	target_page = NUM_BLOCKS;

	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_OK;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(0, seek_page(fh_ptr, target_page));
	EXPECT_EQ(target_page, fh_ptr->cached_page_index);
	EXPECT_EQ((target_page+1)*sizeof(BLOCK_ENTRY_PAGE), fh_ptr->cached_filepos);
	sem_post(&(body_ptr->access_sem));
}
// Test for first block page isn't generated 
TEST_F(seek_pageTest, FirstPageNotExisted) {
	target_page = 0;

	fh_ptr->thisinode = INO_LOOKUP_FILE_DATA_OK_NO_BLOCK_PAGE;
	sem_wait(&(body_ptr->access_sem));

	EXPECT_EQ(0, seek_page(fh_ptr, target_page));
	EXPECT_EQ(target_page, fh_ptr->cached_page_index);
	EXPECT_EQ(0, fh_ptr->cached_filepos);
	sem_post(&(body_ptr->access_sem));
}
*/
/*
	Unittest of seek_page()
 */

/*
	Unittest of advance_block()
 */
 /*
class advance_blockTest : public ::testing::Test {
	protected:

		char *metapath;
		off_t thisfilepos;
		long long entry_index;		

		BLOCK_ENTRY_PAGE *testpage1;
		BLOCK_ENTRY_PAGE *testpage2;

		META_CACHE_ENTRY_STRUCT *body_ptr;	

		virtual void SetUp() {

			FILE *fp;

			metapath = "testpatterns/advance_block_meta_file";
			thisfilepos = 0;
			entry_index = 0;

			testpage1 = (BLOCK_ENTRY_PAGE*)malloc(sizeof(BLOCK_ENTRY_PAGE));
			testpage2 = (BLOCK_ENTRY_PAGE*)malloc(sizeof(BLOCK_ENTRY_PAGE));

			//testpage1->next_page = sizeof(BLOCK_ENTRY_PAGE);
			//testpage2->next_page = 0;
			
			// Create mock meta file 
			fp = fopen(metapath, "wb");
			fwrite(testpage1, sizeof(BLOCK_ENTRY_PAGE), 1, fp);
			fwrite(testpage2, sizeof(BLOCK_ENTRY_PAGE), 1, fp);
			fclose(fp);

			// Open meta file  
			body_ptr = (META_CACHE_ENTRY_STRUCT*)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			body_ptr->fptr = fopen(metapath, "r+");
			setbuf(body_ptr->fptr, NULL);
			body_ptr->meta_opened = TRUE;

		}

                virtual void TearDown() {
			fclose(body_ptr->fptr);
			free(body_ptr);
			free(testpage1);
			free(testpage2);
			remove(metapath);
		}
};
TEST_F(advance_blockTest, FilePosNOTChange) {
	long long temp_index = 0;

	EXPECT_EQ(thisfilepos, advance_block(body_ptr, thisfilepos, &temp_index));
	EXPECT_EQ((entry_index + 1), temp_index);
}
TEST_F(advance_blockTest, AdvanceOK) {
	long long temp_index = MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fileno(body_ptr->fptr), LOCK_EX);

	EXPECT_EQ(sizeof(BLOCK_ENTRY_PAGE), advance_block(body_ptr, thisfilepos, &temp_index));
	EXPECT_EQ(0, temp_index);
}
TEST_F(advance_blockTest, AllocatePageNeeded) {
	long long temp_index = MAX_BLOCK_ENTRIES_PER_PAGE;

	flock(fileno(body_ptr->fptr), LOCK_EX);

	EXPECT_EQ(2*sizeof(BLOCK_ENTRY_PAGE), advance_block(body_ptr, sizeof(BLOCK_ENTRY_PAGE), &temp_index));
	EXPECT_EQ(0, temp_index);
}
*/
/*
	Unittest of advance_block()
 */

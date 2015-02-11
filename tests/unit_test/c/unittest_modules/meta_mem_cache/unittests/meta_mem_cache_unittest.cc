#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
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

extern META_CACHE_HEADER_STRUCT *meta_mem_cache;
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
	body_ptr->inode_num = INO__FETCH_META_PATH_FAIL; 
	/* Test */
	EXPECT_EQ(-1, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, FALSE);
}

TEST_F(meta_cache_open_fileTest, FetchMetaPathError)
{
	/* Create a situation that fetch_meta_path error. */
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = INO__FETCH_META_PATH_ERR; 
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
	body_ptr->inode_num = INO__FETCH_META_PATH_SUCCESS; 
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
	body_ptr->inode_num = INO__FETCH_META_PATH_SUCCESS; 
	/* Test whether it created meta file. */
	EXPECT_EQ(0, meta_cache_open_file(body_ptr));
	EXPECT_EQ(body_ptr->meta_opened, TRUE);
	EXPECT_EQ(0, access(TMP_META_FILE_PATH, F_OK));
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
	body_ptr->inode_num = INO__FETCH_META_PATH_SUCCESS; 
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

/*
	Unit testing for init_meta_cache_headers()
 */
TEST(init_meta_cache_headersTest, InitMetaCacheHeadersSuccess)
{
	int value;
	EXPECT_EQ(0, init_meta_cache_headers());
	for(int count=0 ; count<NUM_META_MEM_CACHE_HEADERS ; count++){
		ASSERT_EQ(NULL, meta_mem_cache[count].last_entry);
		ASSERT_EQ(0, sem_getvalue(&(meta_mem_cache[count].header_sem), &value));
		ASSERT_EQ(1, value);
	}

}
/*
	End of unit testing for init_meta_cache_headers()
 */

/*
	Unit testing for meta_cache_flush_dir_cache()
 */
class meta_cache_flush_dir_cacheTest : public ::testing::Test {
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

TEST_F(meta_cache_flush_dir_cacheTest, EntryCannotBeOpened)
{
	mkdir(TMP_META_DIR, 0700);
	mknod(TMP_META_FILE_PATH, 0400, S_IFREG);
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = INO__FETCH_META_PATH_SUCCESS;
	
	EXPECT_EQ(-1, meta_cache_flush_dir_cache(body_ptr, 0));
	ASSERT_EQ(0, unlink(TMP_META_FILE_PATH));
	ASSERT_EQ(0, rmdir(TMP_META_DIR));
}

TEST_F(meta_cache_flush_dir_cacheTest, FlushDirCacheSuccess)
{
	int page_pos;
	int eindex = 0;
	DIR_ENTRY_PAGE *read_entry = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	
	/* Generate dir_entries */
	srand(time(NULL));
	page_pos = rand() % 10000; /* Random page position in meta file */
	mkdir(TMP_META_DIR, 0700);
	body_ptr->meta_opened = FALSE;
	body_ptr->inode_num = INO__FETCH_META_PATH_SUCCESS;
	body_ptr->dir_entry_cache[eindex] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	for(int i=0; i<MAX_DIR_ENTRIES_PER_PAGE ; i++){
		char tmp_type;
		char tmp_name[10];
		tmp_type = i % 3; /* D_ISDIR, D_ISREG, D_ISLNK */
		sprintf(tmp_name, "mytest%d", i);
		body_ptr->dir_entry_cache[eindex]->dir_entries[i] = DIR_ENTRY{i, "", tmp_type};
		strcpy(body_ptr->dir_entry_cache[eindex]->dir_entries[i].d_name, tmp_name);
	}
	body_ptr->dir_entry_cache[eindex]->this_page_pos = page_pos;
	body_ptr->dir_entry_cache[eindex]->num_entries = MAX_DIR_ENTRIES_PER_PAGE;
	
	/* Check whether the function works  */
	EXPECT_EQ(0, meta_cache_flush_dir_cache(body_ptr, eindex));
	EXPECT_EQ(0, access(TMP_META_FILE_PATH, F_OK));
	EXPECT_EQ(TRUE, body_ptr->meta_opened);
	/* Check meta file content */
	fseek(body_ptr->fptr, page_pos, SEEK_SET);
	fread(read_entry, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	for(int i=0; i<MAX_DIR_ENTRIES_PER_PAGE ; i++){
		char tmp_name[10];
		sprintf(tmp_name, "mytest%d", i);
		ASSERT_STREQ(body_ptr->dir_entry_cache[eindex]->dir_entries[i].d_name, tmp_name);
		ASSERT_EQ(body_ptr->dir_entry_cache[eindex]->dir_entries[i].d_type, i % 3);
		ASSERT_EQ(body_ptr->dir_entry_cache[eindex]->dir_entries[i].d_ino, i);	
	}
	ASSERT_EQ(0, unlink(TMP_META_FILE_PATH));
	ASSERT_EQ(0, rmdir(TMP_META_DIR));
}
/*
	End of unit testing for meta_cache_flush_dir_cache()
 */

/*
	Unit testing for meta_cache_drop_pages()
 */
TEST(meta_cache_drop_pagesTest, SuccessDropAllPages)
{
	META_CACHE_ENTRY_STRUCT *body_ptr = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
	sem_init(&(body_ptr->access_sem), 0, 1);
	sem_wait(&(body_ptr->access_sem));
	body_ptr->meta_opened = TRUE;
	body_ptr->dir_entry_cache[0] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));	
	body_ptr->dir_entry_cache[1] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache_dirty[0] = TRUE;	
	body_ptr->dir_entry_cache_dirty[1] = TRUE;
	/* Test */
	EXPECT_EQ(0, meta_cache_drop_pages(body_ptr));
	EXPECT_EQ(NULL, body_ptr->dir_entry_cache[0]);	
	EXPECT_EQ(NULL, body_ptr->dir_entry_cache[1]);	
	EXPECT_EQ(FALSE, body_ptr->dir_entry_cache_dirty[0]);	
	EXPECT_EQ(FALSE, body_ptr->dir_entry_cache_dirty[1]);	
	/* Free memory */
	sem_post(&(body_ptr->access_sem));
	free(body_ptr->dir_entry_cache[0]);
	free(body_ptr->dir_entry_cache[1]);
	free(body_ptr);
}
/*
	End of unit testing for meta_cache_drop_pages()
 */

/*
	Unit testing for meta_cache_push_dir_page()
 */
class meta_cache_push_dir_pageTest : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			body_ptr = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			test_dir_entry_page = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
			reserved_dir_entry_page = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
			for(int i=0; i<MAX_DIR_ENTRIES_PER_PAGE ; i++){
				char tmp_type;
				char tmp_name[10];
				tmp_type = i % 3; /* D_ISDIR, D_ISREG, D_ISLNK */
				sprintf(tmp_name, "mytest%d", i);
				test_dir_entry_page->dir_entries[i] = DIR_ENTRY{i, "", tmp_type};
				strcpy(test_dir_entry_page->dir_entries[i].d_name, tmp_name);
			}
			for(int i=0; i<MAX_DIR_ENTRIES_PER_PAGE ; i++){
				char tmp_type;
				char tmp_name[20];
				tmp_type = (i+1) % 3; /* D_ISDIR, D_ISREG, D_ISLNK */
				sprintf(tmp_name, "reserved_test%d", i);
				reserved_dir_entry_page->dir_entries[i] = DIR_ENTRY{i, "", tmp_type};
				strcpy(reserved_dir_entry_page->dir_entries[i].d_name, tmp_name);
			}
		}
		virtual void TearDown() 
		{
			free(body_ptr);
			free(test_dir_entry_page);
			free(reserved_dir_entry_page);
		}
		META_CACHE_ENTRY_STRUCT *body_ptr;
		DIR_ENTRY_PAGE *test_dir_entry_page;
		DIR_ENTRY_PAGE *reserved_dir_entry_page;
};

TEST_F(meta_cache_push_dir_pageTest, BothEntryCacheNull)
{
	/* Both NULL */
	body_ptr->dir_entry_cache[0] = NULL;
	body_ptr->dir_entry_cache[1] = NULL;
	/* Test */	
	EXPECT_EQ(0, meta_cache_push_dir_page(body_ptr, test_dir_entry_page));
	EXPECT_EQ(0, memcmp(body_ptr->dir_entry_cache[0], test_dir_entry_page, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(NULL, body_ptr->dir_entry_cache[1]);
}

TEST_F(meta_cache_push_dir_pageTest, OnlyEntry_0_Null)
{
	/* 0 is NULL, 1 is nonempty */
	body_ptr->dir_entry_cache[0] = NULL;
	body_ptr->dir_entry_cache[1] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	memcpy(body_ptr->dir_entry_cache[1], reserved_dir_entry_page, sizeof(DIR_ENTRY_PAGE));
	/* Test */
	EXPECT_EQ(0, meta_cache_push_dir_page(body_ptr, test_dir_entry_page));
	EXPECT_EQ(0, memcmp(test_dir_entry_page, body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(0, memcmp(reserved_dir_entry_page, body_ptr->dir_entry_cache[1], sizeof(DIR_ENTRY_PAGE)));
	
	free(body_ptr->dir_entry_cache[1]);
}

TEST_F(meta_cache_push_dir_pageTest, OnlyEntry_1_Null)
{
	/* 0 is NULL, 1 is nonempty */
	body_ptr->dir_entry_cache[0] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	memcpy(body_ptr->dir_entry_cache[0], reserved_dir_entry_page, sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache[1] = NULL;
	/* Test */
	EXPECT_EQ(0, meta_cache_push_dir_page(body_ptr, test_dir_entry_page));
	EXPECT_EQ(0, memcmp(test_dir_entry_page, body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(0, memcmp(reserved_dir_entry_page, body_ptr->dir_entry_cache[1], sizeof(DIR_ENTRY_PAGE)));
	
	free(body_ptr->dir_entry_cache[0]);
}

TEST_F(meta_cache_push_dir_pageTest, BothNonempty)
{
	/* 0 is NULL, 1 is nonempty */
	body_ptr->dir_entry_cache[0] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache[1] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	memcpy(body_ptr->dir_entry_cache[0], reserved_dir_entry_page, sizeof(DIR_ENTRY_PAGE));
	memcpy(body_ptr->dir_entry_cache[1], test_dir_entry_page, sizeof(DIR_ENTRY_PAGE));
	/* Test */
	EXPECT_EQ(0, meta_cache_push_dir_page(body_ptr, test_dir_entry_page));
	EXPECT_EQ(0, memcmp(test_dir_entry_page, body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(0, memcmp(reserved_dir_entry_page, body_ptr->dir_entry_cache[1], sizeof(DIR_ENTRY_PAGE)));
	
	free(body_ptr->dir_entry_cache[0]);
	free(body_ptr->dir_entry_cache[1]);
}
/*
	End of unit testing for meta_cache_push_dir_page()
 */

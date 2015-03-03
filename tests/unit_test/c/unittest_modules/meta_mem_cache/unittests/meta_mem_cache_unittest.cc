#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
extern "C" {
#include "global.h"
#include "params.h"
#include "fuseop.h"
#include "dir_entry_btree.h"
#include "super_block.h"
#include "meta_mem_cache.h"
#include "mock_tool.h"
}
#include "gtest/gtest.h"

/* 
	Define base class to be derived and extern variable used later 
 */

extern META_CACHE_HEADER_STRUCT *meta_mem_cache;
extern long current_meta_mem_cache_entries;

class BaseClassWithMetaCacheEntry : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			body_ptr = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
		}

		virtual void TearDown() 
		{
			free(body_ptr);
		}
		META_CACHE_ENTRY_STRUCT *body_ptr;
};

/* 
	Unit testing for meta_cache_open_file() 
 */

class meta_cache_open_fileTest : public BaseClassWithMetaCacheEntry {

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

class meta_cache_flush_dir_cacheTest : public BaseClassWithMetaCacheEntry {

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

class meta_cache_drop_pagesTest : public BaseClassWithMetaCacheEntry {

};

TEST_F(meta_cache_drop_pagesTest, CacheNotLocked)
{
	EXPECT_EQ(-1, meta_cache_drop_pages(body_ptr));
}

TEST_F(meta_cache_drop_pagesTest, SuccessDropAllPages)
{

	body_ptr->meta_opened = FALSE;
	body_ptr->dir_entry_cache[0] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));	
	body_ptr->dir_entry_cache[1] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache_dirty[0] = TRUE;	
	body_ptr->dir_entry_cache_dirty[1] = TRUE;
	/* Test */
	sem_wait(&(body_ptr->access_sem));
	EXPECT_EQ(0, meta_cache_drop_pages(body_ptr));
	EXPECT_EQ(NULL, body_ptr->dir_entry_cache[0]);	
	EXPECT_EQ(NULL, body_ptr->dir_entry_cache[1]);	
	EXPECT_EQ(FALSE, body_ptr->dir_entry_cache_dirty[0]);	
	EXPECT_EQ(FALSE, body_ptr->dir_entry_cache_dirty[1]);	
	sem_post(&(body_ptr->access_sem));
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

/*
	Unit testing for meta_cache_lock_entry()
 */
TEST(meta_cache_lock_entryTest, InsertMetaCache)
{
	init_meta_cache_headers();
	META_CACHE_ENTRY_STRUCT *tmp_meta_entry;
	struct stat *expected_stat;
	for(int i=0 ; i<NUM_META_MEM_CACHE_HEADERS ; i++){
		int ino = i*5;
		int sem_val;
		tmp_meta_entry = meta_cache_lock_entry(ino);
		expected_stat = generate_test_stat(ino);
		sem_getvalue(&(tmp_meta_entry->access_sem), &sem_val);
		/* Check lock, number of current cache entries, and stat content */
		ASSERT_EQ(0, sem_val);
		ASSERT_EQ(i+1, current_meta_mem_cache_entries);
		ASSERT_EQ(0, memcmp(&(tmp_meta_entry->this_stat), expected_stat, sizeof(struct stat)));
	}

	/* For each bucket number i, check the linked list of meta_mem_cache[i] */
	for(int i=0 ; i<NUM_META_MEM_CACHE_HEADERS ; i+=5){
		int ino = i;
		META_CACHE_LOOKUP_ENTRY_STRUCT *current = meta_mem_cache[i].last_entry;
		int count = 0;
		while(current != NULL){
			ino = i + NUM_META_MEM_CACHE_HEADERS*count;
			ASSERT_EQ(ino, current->inode_num);
			current = current->prev;
			count++;
		}
	}
	free(meta_mem_cache);
}
/*
	End of unit testing for meta_cache_lock_entry()
 */

/*
	Unit testing for meta_cache_unlock_entry()
 */
class meta_cache_unlock_entryTest : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			body_ptr = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
		}

		virtual void TearDown() 
		{
			free(body_ptr);
		}
		META_CACHE_ENTRY_STRUCT *body_ptr;
};

TEST_F(meta_cache_unlock_entryTest, CacheIsUnlock)
{
	/* Test */
	EXPECT_EQ(-1, meta_cache_unlock_entry(body_ptr));
}

TEST_F(meta_cache_unlock_entryTest, CacheIsLock)
{
	int sem_val;
	sem_wait(&(body_ptr->access_sem));
	/* Test */
	EXPECT_EQ(0, meta_cache_unlock_entry(body_ptr));
	sem_getvalue(&(body_ptr->access_sem), &sem_val);
	EXPECT_EQ(1, sem_val);
	sem_post(&(body_ptr->access_sem));
}
/*
	End of unit testing for meta_cache_unlock_entry()
 */

/*
	Unit testing for meta_cache_remove()
 */
class meta_cache_removeTest : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			init_meta_cache_headers();
			extern sem_t num_entry_sem; 
		}

		virtual void TearDown() 
		{
			free(meta_mem_cache);
		}
};

TEST_F(meta_cache_removeTest, InodeNotFound)
{
	int index;
	int ino;

	for(int i=0 ; i<5 ; i++){
		ino = i*9527; 
		index = ino % NUM_META_MEM_CACHE_HEADERS;
		ASSERT_EQ(0, meta_cache_remove(ino));
		EXPECT_EQ(0, meta_mem_cache[index].num_entries);
		EXPECT_EQ(NULL, meta_mem_cache[index].last_entry);
		EXPECT_EQ(NULL, meta_mem_cache[index].meta_cache_entries);
	}
}

TEST_F(meta_cache_removeTest, RemoveAll)
{
	META_CACHE_LOOKUP_ENTRY_STRUCT *lptr;
	int num_test_buckets = 3;
	int index_list[3] = {1, 5, 60}; /* push into meta_mem_cache[1/5/60], which is magic number. */
	int num_entry = num_test_buckets*50;  /* 50 is number of entry in bucket of linked list meta_mem_cache[1/5/60] */
	int ino_list[num_entry];
	bool is_removed[num_entry];

	/* Generate mock data */
	for(int i=0 ; i<num_entry ; i++){
		ino_list[i] = index_list[i%num_test_buckets] + 
				NUM_META_MEM_CACHE_HEADERS*(i/num_test_buckets); /* Generate inode number */
		/* Init lptr */
		lptr = (META_CACHE_LOOKUP_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
		sem_init(&(lptr->body.access_sem), 0, 1);
		lptr->inode_num = ino_list[i];
		lptr->next = NULL;
		lptr->prev = NULL;
		/* Push into list */
		int index = ino_list[i] % NUM_META_MEM_CACHE_HEADERS;
		if(meta_mem_cache[index].meta_cache_entries != NULL){
			meta_mem_cache[index].meta_cache_entries->prev = lptr;
			lptr->next = meta_mem_cache[index].meta_cache_entries;
		}else{
			meta_mem_cache[index].last_entry = lptr;
			lptr->prev = NULL;
		}
		meta_mem_cache[index].meta_cache_entries = lptr;
		is_removed[i] = false;
	}

	/* Test */
	int random_i;
	int lookup_index;
	for(int i=0 ; i<num_entry ; i++){
		random_i = (i+7) % num_entry;  /* Random start position */
		EXPECT_EQ(0, meta_cache_remove(ino_list[random_i]));
		is_removed[random_i] = true;
		lookup_index = ino_list[random_i] % NUM_META_MEM_CACHE_HEADERS;
		lptr = meta_mem_cache[lookup_index].last_entry;
		for(int count=0 ; lptr!=NULL ; count++){
			if(is_removed[count] == true || 
			   ino_list[count] % NUM_META_MEM_CACHE_HEADERS != lookup_index)
				continue;
			/* Check inode number for remainder */
			ASSERT_EQ(ino_list[count], lptr->inode_num);
			lptr = lptr->prev;
		}
	}
}
/*
	End of unit testing for meta_cache_remove()
 */

/*
	Unit testing for meta_cache_update_file_data()
 */

class meta_cache_update_file_dataTest : public BaseClassWithMetaCacheEntry {	

};

TEST_F(meta_cache_update_file_dataTest, CacheNotLocked)
{
	/* Test for non-locked cache */
	EXPECT_EQ(-1, meta_cache_update_file_data(0, NULL, NULL, NULL, 0, body_ptr));
}

TEST_F(meta_cache_update_file_dataTest, UpdateSuccess)
{
	/* Mock data */
	struct stat test_stat = {0, 10, 5, 6, 88, 91};
	FILE_META_TYPE test_file_meta = {5566, 7788};
	
	body_ptr->file_meta = (FILE_META_TYPE *)malloc(sizeof(FILE_META_TYPE));
	/* Test */
	sem_wait(&(body_ptr->access_sem));
	ASSERT_EQ(0, meta_cache_update_file_data(0, &test_stat, &test_file_meta, NULL, 0, body_ptr));
	EXPECT_EQ(0, memcmp(&test_stat, &(body_ptr->this_stat), sizeof(struct stat)));
	EXPECT_EQ(0, memcmp(&test_file_meta, body_ptr->file_meta, sizeof(FILE_META_TYPE)));
	sem_post(&(body_ptr->access_sem));
}

/*
	End of unit testing for meta_cache_update_file_data()
 */

/*
	Unit testing for meta_cache_lookup_file_data()
 */

class meta_cache_lookup_file_dataTest : public BaseClassWithMetaCacheEntry {

};


TEST_F(meta_cache_lookup_file_dataTest, CacheNotLocked)
{
	/* Test for non-locked cache */
	EXPECT_EQ(-1, meta_cache_lookup_file_data(0, NULL, NULL, NULL, 0, body_ptr));
}

TEST_F(meta_cache_lookup_file_dataTest, LookupSuccess)
{
	struct stat test_stat = {0, 1, 5, 6, 8, 9};
	FILE_META_TYPE test_file_meta = {5566, 7788};
	struct stat *empty_stat = (struct stat *)malloc(sizeof(struct stat));
	FILE_META_TYPE *empty_file_meta = (FILE_META_TYPE *)malloc(sizeof(FILE_META_TYPE));
	/* Mock data */
	body_ptr->file_meta = (FILE_META_TYPE *)malloc(sizeof(FILE_META_TYPE));
	memcpy(&(body_ptr->this_stat), &test_stat, sizeof(struct stat));
	memcpy(body_ptr->file_meta, &test_file_meta, sizeof(FILE_META_TYPE));
	/* Test */
	sem_wait(&(body_ptr->access_sem));	
	ASSERT_EQ(0, meta_cache_lookup_file_data(0, empty_stat, empty_file_meta, NULL, 0, body_ptr));
	EXPECT_EQ(0, memcmp(empty_stat, &test_stat, sizeof(struct stat)));
	EXPECT_EQ(0, memcmp(empty_file_meta, &test_file_meta, sizeof(FILE_META_TYPE)));
	sem_post(&(body_ptr->access_sem));
}

/*
	End of unit testing for meta_cache_lookup_file_data()
 */

/*
	Unit testing for meta_cache_update_dir_data()
 */

class meta_cache_update_dir_dataTest : public BaseClassWithMetaCacheEntry {

};

TEST_F(meta_cache_update_dir_dataTest, CacheNotLocked)
{
	/* Test for non-locked cache */
	EXPECT_EQ(-1, meta_cache_update_dir_data(0, NULL, NULL, NULL, body_ptr));
}

TEST_F(meta_cache_update_dir_dataTest, UpdataSuccess)
{
	/* Mock data */
	struct stat test_stat = {0, 10, 5, 6, 88, 91};
	DIR_META_TYPE test_dir_meta = {5566, 7788, 93, 80, 41};
	/* Test */
	sem_wait(&(body_ptr->access_sem));
	ASSERT_EQ(0, meta_cache_update_dir_data(0, &test_stat, &test_dir_meta, NULL, body_ptr));	
	EXPECT_EQ(0, memcmp(&test_stat, &(body_ptr->this_stat), sizeof(struct stat)));
	EXPECT_EQ(0, memcmp(&test_dir_meta, body_ptr->dir_meta, sizeof(DIR_META_TYPE)));
	sem_post(&(body_ptr->access_sem));
}

TEST_F(meta_cache_update_dir_dataTest, UpdateOnlyDirPage)
{
	/* Mock data */
	DIR_ENTRY_PAGE test_dir_entry_page = {66, {0}, 78, {0}, 93, 80, 41, 5};
	
	sem_wait(&(body_ptr->access_sem));
	/* Hit cache[0] */
	body_ptr->dir_entry_cache[0] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache[1] = NULL;
	body_ptr->dir_entry_cache[0]->this_page_pos = test_dir_entry_page.this_page_pos;
	ASSERT_EQ(0, meta_cache_update_dir_data(0, NULL, NULL, &test_dir_entry_page, body_ptr));
	EXPECT_EQ(0, memcmp(&test_dir_entry_page, body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE)));
	EXPECT_TRUE(body_ptr->dir_entry_cache[1] == NULL);
	
	/* Hit cache[1] */
	free(body_ptr->dir_entry_cache[0]);
	body_ptr->dir_entry_cache[1] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache[0] = NULL;
	body_ptr->dir_entry_cache[1]->this_page_pos = test_dir_entry_page.this_page_pos;
	ASSERT_EQ(0, meta_cache_update_dir_data(0, NULL, NULL, &test_dir_entry_page, body_ptr));
	EXPECT_EQ(0, memcmp(&test_dir_entry_page, body_ptr->dir_entry_cache[1], sizeof(DIR_ENTRY_PAGE)));
	EXPECT_TRUE(body_ptr->dir_entry_cache[0] == NULL);
	
	sem_post(&(body_ptr->access_sem));
}

/*
	End of unit testing for meta_cache_update_dir_data()
 */

/*
	Unit testing for meta_cache_lookup_dir_data()
 */

class meta_cache_lookup_dir_dataTest : public BaseClassWithMetaCacheEntry {

};

TEST_F(meta_cache_lookup_dir_dataTest, CacheNotLocked)
{
	/* Test for non-locked cache */
	EXPECT_EQ(-1, meta_cache_lookup_dir_data(0, NULL, NULL, NULL, body_ptr));
}

TEST_F(meta_cache_lookup_dir_dataTest, LookupSuccess)
{
	/* Mock data */
	struct stat test_stat = {0, 10, 5, 6, 88, 91};
	DIR_META_TYPE test_dir_meta = {5566, 7788, 93, 80, 41};
	struct stat empty_stat;
	DIR_META_TYPE empty_dir_meta;
	
	body_ptr->dir_meta = (DIR_META_TYPE *)malloc(sizeof(DIR_META_TYPE));
	memcpy(&(body_ptr->this_stat), &test_stat, sizeof(struct stat));
	memcpy(body_ptr->dir_meta, &test_dir_meta, sizeof(DIR_META_TYPE));
	sem_wait(&(body_ptr->access_sem));
	/* Test */
	ASSERT_EQ(0, meta_cache_lookup_dir_data(0, &empty_stat, &empty_dir_meta, NULL, body_ptr));
	EXPECT_EQ(0, memcmp(&empty_stat, &test_stat, sizeof(struct stat)));
	EXPECT_EQ(0, memcmp(&empty_dir_meta, &test_dir_meta, sizeof(DIR_META_TYPE)));
	
	sem_post(&(body_ptr->access_sem));
}

TEST_F(meta_cache_lookup_dir_dataTest, UpdataOnlyDirPage)
{
	/* Mock data */
	DIR_ENTRY_PAGE test_dir_entry_page = {66, {0}, 78, {0}, 93, 80, 41, 5};
	DIR_ENTRY_PAGE empty_dir_entry_page;
	
	sem_wait(&(body_ptr->access_sem));
	/* Mock data for cache[0] */
	empty_dir_entry_page.this_page_pos = test_dir_entry_page.this_page_pos;
	body_ptr->dir_entry_cache[0] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	memcpy(body_ptr->dir_entry_cache[0], &test_dir_entry_page, sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache[1] = NULL;
	/* Test cache[0] */
	ASSERT_EQ(0, meta_cache_lookup_dir_data(0, NULL, NULL, &empty_dir_entry_page, body_ptr));
	EXPECT_EQ(0, memcmp(&test_dir_entry_page, &empty_dir_entry_page, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_TRUE(body_ptr->dir_entry_cache[1] == NULL);
	
	/* Mock data for cache[1] */
	free(body_ptr->dir_entry_cache[0]);
	body_ptr->dir_entry_cache[1] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	memcpy(body_ptr->dir_entry_cache[1], &test_dir_entry_page, sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache[0] = NULL;
	/* Test cacahe[1] */
	ASSERT_EQ(0, meta_cache_lookup_dir_data(0, NULL, NULL, &empty_dir_entry_page, body_ptr));
	EXPECT_EQ(0, memcmp(&test_dir_entry_page, &empty_dir_entry_page, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_TRUE(body_ptr->dir_entry_cache[0] == NULL);
	
	sem_post(&(body_ptr->access_sem));
}

/*
	Unit testing for flush_single_entry()
 */

class flush_single_entryTest : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			body_ptr = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			mkdir(TMP_META_DIR, 0700);
			mknod(TMP_META_FILE_PATH, 0700, S_IFREG);
			body_ptr->fptr = fopen(TMP_META_FILE_PATH, "rw+");
			body_ptr->meta_opened = TRUE;
			body_ptr->something_dirty = TRUE;
			sem_init(&(body_ptr->access_sem), 0, 1);
			initialize_mock_meta();
		}

		virtual void TearDown() 
		{
			fclose(body_ptr->fptr);
			unlink(TMP_META_FILE_PATH);
			rmdir(TMP_META_DIR);			
			free(body_ptr);
		}

		META_CACHE_ENTRY_STRUCT *body_ptr;
		DIR_META_TYPE test_dir_meta;
		FILE_META_TYPE test_file_meta;
		DIR_ENTRY_PAGE test_dir_entry[2];
		struct stat test_file_stat;
		struct stat test_dir_stat;
	private:
		void initialize_mock_meta()
		{
			DIR_META_TYPE dir_meta = {5566, 7788, 99, 77, 33};
			FILE_META_TYPE file_meta = {93, 80};
			DIR_ENTRY_PAGE dir_entry1 = {0, {0}, 1122, {0}, 66, 77, 76, 5};
			DIR_ENTRY_PAGE dir_entry2 = {2, {0}, 4500, {0}, 55, 88, 97, 22};
			
			test_file_stat = {1, 2, 0, 3, 4, 5, 6};
			test_dir_stat = {6, 7, 0, 8, 9, 10, 11};
			test_file_stat.st_mode = S_IFREG;
			test_dir_stat.st_mode = S_IFDIR;
			test_file_meta = file_meta;
			test_dir_meta = dir_meta;
			test_dir_entry[0] = dir_entry1;
			test_dir_entry[1] = dir_entry2;
			test_dir_entry[0].this_page_pos = 4 * sizeof(DIR_ENTRY_PAGE);
			test_dir_entry[1].this_page_pos = 9 * sizeof(DIR_ENTRY_PAGE);

		}
};

TEST_F(flush_single_entryTest, CacheNotLock)
{
	EXPECT_EQ(-1, flush_single_entry(body_ptr));
}

TEST_F(flush_single_entryTest, FlushFileMeta)
{
	struct stat verified_stat;
	FILE_META_TYPE verified_file_meta;
	/* Mock data */	
	body_ptr->stat_dirty = TRUE;
	body_ptr->meta_dirty = TRUE;
	body_ptr->something_dirty = TRUE;
	body_ptr->file_meta = (FILE_META_TYPE *)malloc(sizeof(FILE_META_TYPE));
	memcpy(&(body_ptr->this_stat), &test_file_stat, sizeof(struct stat));
	memcpy(body_ptr->file_meta, &test_file_meta, sizeof(FILE_META_TYPE));	
	/* Run function and read file */
	sem_wait(&(body_ptr->access_sem));
	ASSERT_EQ(0, flush_single_entry(body_ptr));
	fseek(body_ptr->fptr, 0, SEEK_SET);
	fread(&verified_stat, sizeof(struct stat), 1, body_ptr->fptr);
	fseek(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
	fread(&verified_file_meta, sizeof(FILE_META_TYPE), 1, body_ptr->fptr);
	/* Test */	
	EXPECT_EQ(0, memcmp(&verified_stat, &(body_ptr->this_stat), sizeof(struct stat)));
	EXPECT_EQ(0, memcmp(&verified_file_meta, body_ptr->file_meta, sizeof(FILE_META_TYPE)));
	EXPECT_EQ(FALSE, body_ptr->stat_dirty);
	EXPECT_EQ(FALSE, body_ptr->meta_dirty);
	EXPECT_EQ(FALSE, body_ptr->something_dirty);
	/* Free resource */	
	sem_post(&(body_ptr->access_sem));
	free(body_ptr->file_meta);
}

TEST_F(flush_single_entryTest, FlushDirMeta)
{
	struct stat verified_stat;
	DIR_META_TYPE verified_dir_meta;
	DIR_ENTRY_PAGE verified_dir_entry[2];
	/* Mock data */
	body_ptr->stat_dirty = TRUE;
	body_ptr->meta_dirty = TRUE;
	body_ptr->something_dirty = TRUE;
	body_ptr->dir_entry_cache_dirty[0] = TRUE;
	body_ptr->dir_entry_cache_dirty[1] = TRUE;
	body_ptr->dir_meta = (DIR_META_TYPE *)malloc(sizeof(DIR_META_TYPE));
	body_ptr->dir_entry_cache[0] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	body_ptr->dir_entry_cache[1] = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	memcpy(&(body_ptr->this_stat), &test_dir_stat, sizeof(struct stat));
	memcpy(body_ptr->dir_meta, &test_dir_meta, sizeof(DIR_META_TYPE));
	memcpy(body_ptr->dir_entry_cache[0], &test_dir_entry[0], sizeof(DIR_ENTRY_PAGE));
	memcpy(body_ptr->dir_entry_cache[1], &test_dir_entry[1], sizeof(DIR_ENTRY_PAGE));
	/* Run function and read file */
	sem_wait(&(body_ptr->access_sem));
	ASSERT_EQ(0, flush_single_entry(body_ptr));
	fseek(body_ptr->fptr, 0, SEEK_SET);
	fread(&verified_stat, sizeof(struct stat), 1, body_ptr->fptr);
	fseek(body_ptr->fptr, sizeof(struct stat), SEEK_SET);
	fread(&verified_dir_meta, sizeof(DIR_META_TYPE), 1, body_ptr->fptr);
	fseek(body_ptr->fptr, body_ptr->dir_entry_cache[1]->this_page_pos, SEEK_SET);
	fread(&verified_dir_entry[1], 1, sizeof(DIR_ENTRY_PAGE), body_ptr->fptr);
	fseek(body_ptr->fptr, body_ptr->dir_entry_cache[0]->this_page_pos, SEEK_SET);
	fread(&verified_dir_entry[0], 1, sizeof(DIR_ENTRY_PAGE), body_ptr->fptr);
	/* Test */	
	EXPECT_EQ(0, memcmp(&verified_stat, &(body_ptr->this_stat), sizeof(struct stat)));
	EXPECT_EQ(0, memcmp(&verified_dir_meta, body_ptr->dir_meta, sizeof(DIR_META_TYPE)));
	EXPECT_EQ(0, memcmp(&verified_dir_entry[0], body_ptr->dir_entry_cache[0], sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(0, memcmp(&verified_dir_entry[1], body_ptr->dir_entry_cache[1], sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(FALSE, body_ptr->stat_dirty);
	EXPECT_EQ(FALSE, body_ptr->meta_dirty);
	EXPECT_EQ(FALSE, body_ptr->something_dirty);
	/* Free resource */	
	sem_post(&(body_ptr->access_sem));
	free(body_ptr->dir_meta);
	free(body_ptr->dir_entry_cache[0]);
	free(body_ptr->dir_entry_cache[1]);
}

/*
	End of unit testing for flush_single_entry()
 */

/*
	Unit testing for expire_meta_mem_cache_entry()
 */

class expire_meta_mem_cache_entryTest : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			init_meta_cache_headers();
			extern sem_t num_entry_sem; 
			META_CACHE_LOOKUP_ENTRY_STRUCT *lptr;
			
			for (int i=0 ; i<10000 ; i+=4){
				lptr = (META_CACHE_LOOKUP_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
				int index = i % NUM_META_MEM_CACHE_HEADERS;
				init_lookup_entry(lptr, i);
				push_lookup_entry(lptr, index);
			}
		}

		virtual void TearDown() 
		{
			for (int i=0 ; i<NUM_META_MEM_CACHE_HEADERS ; i++) {
				META_CACHE_LOOKUP_ENTRY_STRUCT *now = meta_mem_cache[i].meta_cache_entries;
				META_CACHE_LOOKUP_ENTRY_STRUCT *next;
				while (now != NULL) {
					next = now->next;
					free(now);
					now = next;
				}
			}
			free(meta_mem_cache);
		}
	
		void init_lookup_entry(META_CACHE_LOOKUP_ENTRY_STRUCT *lptr, const int ino_num)
		{
			lptr->body.something_dirty = FALSE;
			lptr->inode_num = ino_num;
			lptr->prev = NULL;
			lptr->next = NULL;
			lptr->body.dir_meta = NULL;
			lptr->body.file_meta = NULL;
			lptr->body.dir_entry_cache[0] = NULL;
			lptr->body.dir_entry_cache[1] = NULL;
			gettimeofday(&(lptr->body.last_access_time), NULL);
			sem_init(&(lptr->body.access_sem), 0, 1);
		}

		void push_lookup_entry(META_CACHE_LOOKUP_ENTRY_STRUCT *lptr, const int index)
		{
			if (meta_mem_cache[index].meta_cache_entries != NULL) {
				meta_mem_cache[index].meta_cache_entries->prev = lptr;
				lptr->next = meta_mem_cache[index].meta_cache_entries;
			} else {
				meta_mem_cache[index].last_entry = lptr;					
			}
			meta_mem_cache[index].meta_cache_entries = lptr;
		}
};

TEST_F(expire_meta_mem_cache_entryTest, ExpireNothing)
{
	/* Expire nothing because period time < 0.5 sec */
	ASSERT_EQ(-1, expire_meta_mem_cache_entry());
}

TEST_F(expire_meta_mem_cache_entryTest, ExpireEntrySuccess)
{
	META_CACHE_LOOKUP_ENTRY_STRUCT *lptr;
	META_CACHE_LOOKUP_ENTRY_STRUCT *now;
	unsigned expired_ino_num;
	unsigned index;
	/* Test the function for 10 times */
	for (int test_times=0 ; test_times<10 ; test_times++) {	
		/* Generate mock entry and push into meta_mem_cache[] */
		lptr = (META_CACHE_LOOKUP_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_LOOKUP_ENTRY_STRUCT));
		srandom(time(NULL));
		expired_ino_num = random()%5000 + 10000; /* An entry to be expired */
		index = expired_ino_num % NUM_META_MEM_CACHE_HEADERS;
		init_lookup_entry(lptr, expired_ino_num);
		lptr->body.last_access_time.tv_sec -= 3;
		push_lookup_entry(lptr, index);
		/* Test whether the entry is really expired */
		ASSERT_EQ(0, expire_meta_mem_cache_entry());
		now = meta_mem_cache[index].meta_cache_entries;
		while (now != NULL) {
			ASSERT_TRUE(now->inode_num != expired_ino_num);
			now = now->next;
		}
		/* Check lock */
		for (int i=0 ; i<NUM_META_MEM_CACHE_HEADERS ; i++) {
			int val = -1;
			sem_getvalue(&(meta_mem_cache[i].header_sem), &val);
			ASSERT_EQ(1, val);
		}
	}
}

/*
	End of unit testing for expire_meta_mem_cache_entry()
 */

/*
	Unit testing for meta_cache_seek_dir_entry()
 */
class meta_cache_seek_dir_entryTest : public ::testing::Test {
	protected:
		virtual void SetUp() 
		{
			body_ptr = (META_CACHE_ENTRY_STRUCT *)malloc(sizeof(META_CACHE_ENTRY_STRUCT));
			sem_init(&(body_ptr->access_sem), 0, 1);
			/* Mock dir_entry_page */
			test_dir_entry = DIR_ENTRY{10, "test_name", 0};
			test_dir_entry_page = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
			test_dir_entry_page->num_entries = 1;
			test_dir_entry_page->dir_entries[0] = test_dir_entry; 
			
			test_dir_entry2 = DIR_ENTRY{20, "test_name2", 0};
			test_dir_entry_page2 = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
			test_dir_entry_page2->num_entries = 1;
			test_dir_entry_page2->dir_entries[0] = test_dir_entry2; 
		}

		virtual void TearDown() 
		{
			free(test_dir_entry_page);
			free(test_dir_entry_page2);
			free(body_ptr);
		}

		META_CACHE_ENTRY_STRUCT *body_ptr;
		DIR_ENTRY_PAGE *test_dir_entry_page, *test_dir_entry_page2;
		DIR_ENTRY test_dir_entry, test_dir_entry2;
};

TEST_F(meta_cache_seek_dir_entryTest, CacheNotLocked)
{
	DIR_ENTRY_PAGE verified_dir_entry_page;
	int verified_index;
	ASSERT_EQ(-1, meta_cache_seek_dir_entry(0, &verified_dir_entry_page, &verified_index, "test_name", body_ptr));
}

TEST_F(meta_cache_seek_dir_entryTest, Success_Found_In_Cache)
{
	DIR_ENTRY_PAGE *verified_dir_entry_page;
	int verified_index;
	/* Mock data is in cache[0] */
	verified_dir_entry_page = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	verified_index = -1;
	body_ptr->dir_entry_cache[0] = test_dir_entry_page;
	body_ptr->dir_entry_cache[1] = NULL;
	/* Test for successing found in cache[0] */
	sem_wait(&(body_ptr->access_sem));
	ASSERT_EQ(0, meta_cache_seek_dir_entry(0, verified_dir_entry_page, &verified_index, "test_name", body_ptr));
	EXPECT_EQ(0, memcmp(verified_dir_entry_page, test_dir_entry_page, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_NE(0, memcmp(verified_dir_entry_page, test_dir_entry_page2, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(0, verified_index);
	
	/* Mock data is in cache[1] */
	verified_index = -1;
	body_ptr->dir_entry_cache[1] = test_dir_entry_page2;
	/* Test for successing found in cache[0] */
	ASSERT_EQ(0, meta_cache_seek_dir_entry(0, verified_dir_entry_page, &verified_index, "test_name2", body_ptr));
	EXPECT_EQ(0, memcmp(verified_dir_entry_page, test_dir_entry_page2, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_NE(0, memcmp(verified_dir_entry_page, test_dir_entry_page, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(0, verified_index);
	
	sem_post(&(body_ptr->access_sem));
	free(verified_dir_entry_page);

}
 
TEST_F(meta_cache_seek_dir_entryTest, Success_Found_From_Rootpage)
{	
	DIR_ENTRY_PAGE *verified_dir_entry_page;
	int verified_index;
	
	mkdir(TMP_META_DIR, 0700);
	mknod(TMP_META_FILE_PATH, 0700, S_IFREG);
	body_ptr->fptr = fopen(TMP_META_FILE_PATH, "rw+");
	ASSERT_TRUE(body_ptr->fptr != NULL);
	fseek(body_ptr->fptr, 10, SEEK_SET);
	fwrite(test_dir_entry_page, sizeof(DIR_ENTRY_PAGE), 1, body_ptr->fptr);
	body_ptr->meta_opened = TRUE;
	body_ptr->dir_meta = (DIR_META_TYPE *)malloc(sizeof(DIR_META_TYPE));
	body_ptr->dir_meta->root_entry_page = 10;
	body_ptr->dir_entry_cache[0] = NULL;
	body_ptr->dir_entry_cache[1] = NULL;
	verified_dir_entry_page = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
	verified_index = -1;

	/* Test */
	sem_wait(&(body_ptr->access_sem));
	ASSERT_EQ(0, meta_cache_seek_dir_entry(0, verified_dir_entry_page, &verified_index, "test_name", body_ptr));
	EXPECT_EQ(0, memcmp(verified_dir_entry_page, test_dir_entry_page, sizeof(DIR_ENTRY_PAGE)));
	EXPECT_EQ(0, verified_index);
	sem_post(&(body_ptr->access_sem));

	/* Free resource */
	fclose(body_ptr->fptr);
	unlink(TMP_META_FILE_PATH);
	rmdir(TMP_META_DIR);
	free(verified_dir_entry_page);
	free(body_ptr->dir_meta);
}



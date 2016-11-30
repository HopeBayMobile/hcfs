#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

extern "C" {
#include "meta_iterator.h"
#include "fuseop.h"
#include "meta.h"
}
#include "gtest/gtest.h"

class meta_iteratorEnvironment : public ::testing::Environment {
public:
	void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
			calloc(sizeof(SYSTEM_CONF_STRUCT), 1);
		MAX_BLOCK_SIZE = 1048576;
	}
	void TearDown()
	{
		free(system_config);
	}
};

::testing::Environment* const upload_env =
	::testing::AddGlobalTestEnvironment(new meta_iteratorEnvironment);

extern int32_t RETURN_PAGE_NOT_FOUND;

/**
 * Unittest for init_block_iter()
 */
class init_block_iterTest : public ::testing::Test {
protected:
	void SetUp()
	{
		system("rm -rf iterator_test");
		mkdir("iterator_test", 0777);
	}

	void TearDown()
	{
		system("rm -rf iterator_test");
	}
};

TEST_F(init_block_iterTest, FileEmpty)
{
	FILE *fptr;
	FILE_BLOCK_ITERATOR *iter;

	fptr = fopen("iterator_test/file_empty", "w");
	ASSERT_TRUE(fptr != NULL) << "errcode: " << errno;

	iter = init_block_iter(fptr);
	ASSERT_TRUE(iter == NULL);

	fclose(fptr);
}

TEST_F(init_block_iterTest, InitSuccess)
{
	FILE *fptr;
	FILE_BLOCK_ITERATOR *iter;
	HCFS_STAT hcfsstat = {1, 2, 3};
	FILE_META_TYPE filemeta = {4, 5, 6};

	hcfsstat.size = MAX_BLOCK_SIZE + 100;
	fptr = fopen("iterator_test/file_empty", "w+");
	ASSERT_TRUE(fptr != NULL) << "errcode: " << errno;
	setbuf(fptr, NULL);

	fwrite(&hcfsstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);

	iter = init_block_iter(fptr);
	ASSERT_TRUE(iter != NULL);

	EXPECT_EQ(2, iter->total_blocks);
	EXPECT_EQ(0, memcmp(&hcfsstat, &(iter->filestat), sizeof(HCFS_STAT)));
	EXPECT_EQ(0, memcmp(&filemeta, &(iter->filemeta),
			sizeof(FILE_META_TYPE)));
	destroy_block_iter(iter);
	fclose(fptr);
}
/**
 * End unittest for init_block_iter()
 */

/**
 * Unittest for next_block()
 */
class next_blockTest : public ::testing::Test {
protected:
	void SetUp()
	{
		mkdir("iterator_test", 0777);
		RETURN_PAGE_NOT_FOUND = 0;
	}

	void TearDown()
	{
		system("rm -rf iterator_test");
		RETURN_PAGE_NOT_FOUND = 0;
	}
};

TEST_F(next_blockTest, TraverseAllSuccess)
{
	FILE *fptr;
	FILE_BLOCK_ITERATOR *iter;
	HCFS_STAT hcfsstat = {1, 2, 3};
	FILE_META_TYPE filemeta = {4, 5, 6};
	BLOCK_ENTRY_PAGE page = {0};
	int32_t counter = 0;

	/* Status for each block in a page is either LDISK or NONE */
	for (int i = 0; i < MAX_BLOCK_ENTRIES_PER_PAGE; i++) {
		if (i % 2)
			page.block_entries[i].status = ST_LDISK;
		else
			page.block_entries[i].status = ST_NONE;
	}
	/* 3 page */
	hcfsstat.size = MAX_BLOCK_SIZE * MAX_BLOCK_ENTRIES_PER_PAGE * 3;
	fptr = fopen("iterator_test/file_test", "w+");
	ASSERT_TRUE(fptr != NULL) << "errcode: " << errno;
	setbuf(fptr, NULL);
	
	fwrite(&hcfsstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	for (int i = 0; i < 3; i++) /* Write 3 page */
		fwrite(&page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);

	iter = init_block_iter(fptr);
	ASSERT_TRUE(iter != NULL);
	EXPECT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, iter->total_blocks);

	/* Test */
	counter = 0;
	while (iter_next(iter)) {
		int64_t expected_page_no = counter / MAX_BLOCK_ENTRIES_PER_PAGE;
		int64_t expected_entry_idx = counter % MAX_BLOCK_ENTRIES_PER_PAGE;
		int64_t expected_block_no = counter;
		int64_t expected_page_pos = sizeof(HCFS_STAT) +
				sizeof(FILE_META_TYPE) +
				expected_page_no * sizeof(BLOCK_ENTRY_PAGE);

		ASSERT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, iter->total_blocks);
		ASSERT_EQ(expected_page_no, iter->now_page_no);
		ASSERT_EQ(expected_entry_idx, iter->e_index);
		ASSERT_EQ(expected_block_no, iter->now_block_no);
		ASSERT_EQ(expected_page_pos, iter->page_pos);
		if (counter % 2)
			ASSERT_EQ(ST_LDISK, iter->now_bentry->status);
		else
			ASSERT_EQ(ST_NONE, iter->now_bentry->status);
		counter++;
		ASSERT_EQ(0, memcmp(&(iter->page), &page, sizeof(BLOCK_ENTRY_PAGE)));
	}
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(counter, MAX_BLOCK_ENTRIES_PER_PAGE * 3);

	/* Free resource */
	destroy_block_iter(iter);
	fclose(fptr);
}

TEST_F(next_blockTest, TraverseTruncatedFile)
{
	FILE *fptr;
	FILE_BLOCK_ITERATOR *iter;
	HCFS_STAT hcfsstat = {1, 2, 3};
	FILE_META_TYPE filemeta = {4, 5, 6};
	BLOCK_ENTRY_PAGE page = {0};
	int32_t counter = 0;

	RETURN_PAGE_NOT_FOUND = 0; /* Control returned value of seek_page2() */

	/* Status for each block in a page is either LDISK or NONE */
	for (int i = 0; i < MAX_BLOCK_ENTRIES_PER_PAGE; i++) {
		if (i % 2)
			page.block_entries[i].status = ST_LDISK;
		else
			page.block_entries[i].status = ST_NONE;
	}
	/* 3 page */
	hcfsstat.size = MAX_BLOCK_SIZE * MAX_BLOCK_ENTRIES_PER_PAGE * 3;
	fptr = fopen("iterator_test/file_empty", "w+");
	ASSERT_TRUE(fptr != NULL) << "errcode: " << errno;
	setbuf(fptr, NULL);
	
	fwrite(&hcfsstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	fwrite(&page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);

	iter = init_block_iter(fptr);
	ASSERT_TRUE(iter != NULL);
	EXPECT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, iter->total_blocks);

	/* Test */
	counter = 0;
	while (iter_next(iter)) {
		int64_t expected_page_no = counter / MAX_BLOCK_ENTRIES_PER_PAGE;
		int64_t expected_entry_idx = counter % MAX_BLOCK_ENTRIES_PER_PAGE;
		int64_t expected_block_no = counter;
		int64_t expected_page_pos = sizeof(HCFS_STAT) +
				sizeof(FILE_META_TYPE) +
				expected_page_no * sizeof(BLOCK_ENTRY_PAGE);

		ASSERT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, iter->total_blocks);
		ASSERT_EQ(expected_page_no, iter->now_page_no);
		ASSERT_EQ(expected_entry_idx, iter->e_index);
		ASSERT_EQ(expected_block_no, iter->now_block_no);
		ASSERT_EQ(expected_page_pos, iter->page_pos);
		if (counter % 2)
			ASSERT_EQ(ST_LDISK, iter->now_bentry->status);
		else
			ASSERT_EQ(ST_NONE, iter->now_bentry->status);
		counter++;
		/* Let seek_page2() return 0 at the end of page1 */
		if (counter == MAX_BLOCK_ENTRIES_PER_PAGE)
			RETURN_PAGE_NOT_FOUND = 1;
	}
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(counter, MAX_BLOCK_ENTRIES_PER_PAGE);

	/* Free resource */
	destroy_block_iter(iter);
	fclose(fptr);
}
/**
 * End of unittest for next_block()
 */

/**
 * Unittest for begin_block()
 */
class begin_blockTest : public ::testing::Test {
protected:
	void SetUp()
	{
		mkdir("iterator_test", 0777);
	}

	void TearDown()
	{
		system("rm -rf iterator_test");
	}
};

TEST_F(begin_blockTest, GotoFistBlockSuccess)
{
	FILE *fptr;
	FILE_BLOCK_ITERATOR *iter;
	HCFS_STAT hcfsstat = {1, 2, 3};
	FILE_META_TYPE filemeta = {4, 5, 6};
	BLOCK_ENTRY_PAGE page = {0};
	int32_t counter = 0;

	/* Status for each block in a page is either LDISK or NONE */
	for (int i = 0; i < MAX_BLOCK_ENTRIES_PER_PAGE; i++) {
		if (i % 2)
			page.block_entries[i].status = ST_LDISK;
		else
			page.block_entries[i].status = ST_NONE;
	}

	/* 3 page */
	hcfsstat.size = MAX_BLOCK_SIZE * MAX_BLOCK_ENTRIES_PER_PAGE * 3;
	fptr = fopen("iterator_test/file_test", "w+");
	ASSERT_TRUE(fptr != NULL) << "errcode: " << errno;
	setbuf(fptr, NULL);

	fwrite(&hcfsstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	for (int i = 0; i < 3; i++) /* Write 3 page */
		fwrite(&page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);

	iter = init_block_iter(fptr);
	ASSERT_TRUE(iter != NULL);
	EXPECT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, iter->total_blocks);

	/* Test: First traverse all entries, and then jump to first one */
	counter = 0;
	while (iter_next(iter))
		counter++;
	ASSERT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, counter);

	ASSERT_TRUE(iter_begin(iter) != NULL);
	EXPECT_EQ(0, iter->now_page_no);
	EXPECT_EQ(0, iter->e_index);
	EXPECT_EQ(0, iter->now_block_no);
	EXPECT_EQ(sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE), iter->page_pos);
	EXPECT_EQ(ST_NONE, iter->now_bentry->status);

	/* Free resource */
	destroy_block_iter(iter);
	fclose(fptr);
}
/**
 * End of unittest for begin_block()
 */

/**
 * Unittest for goto_block()
 */
class jump_blockTest : public ::testing::Test {
protected:
	void SetUp()
	{
		mkdir("iterator_test", 0777);
	}

	void TearDown()
	{
		system("rm -rf iterator_test");
	}
};

TEST_F(jump_blockTest, JumpSuccess)
{
	FILE *fptr;
	FILE_BLOCK_ITERATOR *iter;
	HCFS_STAT hcfsstat = {1, 2, 3};
	FILE_META_TYPE filemeta = {4, 5, 6};
	BLOCK_ENTRY_PAGE page = {0};
	int32_t counter = 0;

	/* Status for each block in a page is either LDISK or NONE */
	for (int i = 0; i < MAX_BLOCK_ENTRIES_PER_PAGE; i++) {
		if (i % 2)
			page.block_entries[i].status = ST_LDISK;
		else
			page.block_entries[i].status = ST_NONE;
	}

	/* 3 page */
	hcfsstat.size = MAX_BLOCK_SIZE * MAX_BLOCK_ENTRIES_PER_PAGE * 3;
	fptr = fopen("iterator_test/file_test", "w+");
	ASSERT_TRUE(fptr != NULL) << "errcode: " << errno;
	setbuf(fptr, NULL);

	fwrite(&hcfsstat, sizeof(HCFS_STAT), 1, fptr);
	fwrite(&filemeta, sizeof(FILE_META_TYPE), 1, fptr);
	for (int i = 0; i < 3; i++) /* Write 3 page */
		fwrite(&page, sizeof(BLOCK_ENTRY_PAGE), 1, fptr);

	iter = init_block_iter(fptr);
	ASSERT_TRUE(iter != NULL);
	EXPECT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, iter->total_blocks);

	/* Test: First traverse all entries, and then jump to first one */
	counter = 0;
	while (iter_next(iter))
		counter++;
	ASSERT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3, counter);

	/* Jump to idx = 0 */
	ASSERT_TRUE(iter_jump(iter, 0) != NULL);
	EXPECT_EQ(0, iter->now_page_no);
	EXPECT_EQ(0, iter->e_index);
	EXPECT_EQ(0, iter->now_block_no);
	EXPECT_EQ(sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE), iter->page_pos);
	EXPECT_EQ(ST_NONE, iter->now_bentry->status);

	/* Jump to idx = last one */
	ASSERT_TRUE(iter_jump(iter, MAX_BLOCK_ENTRIES_PER_PAGE * 3 - 1) != NULL);
	EXPECT_EQ(2, iter->now_page_no);
	EXPECT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE - 1, iter->e_index);
	EXPECT_EQ(MAX_BLOCK_ENTRIES_PER_PAGE * 3 - 1, iter->now_block_no);
	EXPECT_EQ(sizeof(HCFS_STAT) + sizeof(FILE_META_TYPE) +
			2 * sizeof(BLOCK_ENTRY_PAGE), iter->page_pos);
	EXPECT_EQ(ST_LDISK, iter->now_bentry->status);

	/* Jump out of index */
	ASSERT_TRUE(iter_jump(iter, MAX_BLOCK_ENTRIES_PER_PAGE * 3) == NULL);
	EXPECT_EQ(ENOENT, errno);

	/* Free resource */
	destroy_block_iter(iter);
	fclose(fptr);
}
/**
 * End of unittest for goto_block()
 */

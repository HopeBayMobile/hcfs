/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

extern "C" {
#include "meta_iterator.h"
#include "fuseop.h"
#include "meta.h"
#include "hash_list_struct.h"
}
#include "gtest/gtest.h"
#include "../../fff.h"
DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(void *, calloc, size_t, size_t);
void *(*const real_calloc)(size_t nmemb, size_t size) =
    (void *(*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");

FAKE_VALUE_FUNC(int, fseek, FILE *, long, int);
int (*const real_fseek)(FILE *stream, long offset, int whence) =
    (int(*)(FILE *, long, int))dlsym(RTLD_NEXT, "fseek");

extern int32_t fseek_success;
extern int32_t calloc_success;
void *fake_calloc(size_t nmemb, size_t size)
{
	if (calloc_success)
		return (void *)1;

	errno = ENOMEM;
	return NULL;
}

int fake_fseek(FILE *stream, long offset, int whence)
{
	if (fseek_success)
		return real_fseek(stream, offset, whence);
	
	errno = EIO;
	return -1;
}

void reset_all_fake(void)
{
	RESET_FAKE(calloc);
	RESET_FAKE(fseek);
	FFF_RESET_HISTORY();
	calloc_fake.custom_fake = real_calloc;
	calloc_success = 1;
	fseek_fake.custom_fake = real_fseek;
	fseek_success = 1;
}

class meta_iteratorEnvironment : public ::testing::Environment {
public:
	void SetUp()
	{
		reset_all_fake();
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
		reset_all_fake();
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
		reset_all_fake();
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
		reset_all_fake();
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
		reset_all_fake();
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

/**
 * Unittest for init_hashlist_iter()
 */
typedef struct {
	int32_t key;
} test_key_t;

typedef struct {
	int32_t data;
} test_data_t;

#define HASH_SIZE 256

static int32_t _hash_ftn(const hash_key_t *key)
{
	return (((test_key_t *)key)->key % HASH_SIZE);
}

int32_t _key_cmp_ftn(const hash_key_t *key1, const hash_key_t *key2)
{
	return ((test_key_t *)key1)->key - ((test_key_t *)key2)->key;
}

class init_hashlist_iterTest : public ::testing::Test {
protected:
	HASH_LIST *hash_list;

	void SetUp()
	{
		reset_all_fake();
		hash_list =
		    create_hash_list(_hash_ftn, _key_cmp_ftn, NULL, HASH_SIZE,
				     sizeof(test_key_t), sizeof(test_data_t));
	}

	void TearDown()
	{
		destroy_hash_list(hash_list);
	}
};

TEST_F(init_hashlist_iterTest, InitFail_NOMEM)
{
	HASH_LIST_ITERATOR *iter;

	ASSERT_TRUE(hash_list != NULL);
	/* Set fake function and run. */
	calloc_fake.custom_fake = fake_calloc;
	calloc_success = 0;
	iter = init_hashlist_iter(hash_list);
	EXPECT_EQ(NULL, iter);
	EXPECT_EQ(ENOMEM, errno);
}

TEST_F(init_hashlist_iterTest, InitSuccess)
{
	HASH_LIST_ITERATOR *iter;

	ASSERT_TRUE(hash_list != NULL);
	iter = init_hashlist_iter(hash_list);

	/* Verify */
	ASSERT_TRUE(iter != NULL);
	EXPECT_EQ(iter->hash_list, hash_list);
	EXPECT_EQ(iter->base.begin, begin_entry);
	EXPECT_EQ(iter->base.next, next_entry);
	EXPECT_TRUE(iter->base.jump == NULL);
	EXPECT_EQ(iter->now_bucket_idx, -1);
	EXPECT_EQ(iter->now_node, NULL);
	EXPECT_EQ(iter->now_key, NULL);
	EXPECT_EQ(iter->now_data, NULL);
}
/**
 * End unittest for init_hashlist_iter()
 */

/**
 * Unittest for next_entry()
 */
class next_entryTest : public ::testing::Test {
protected:
	HASH_LIST *hash_list;
	HASH_LIST_ITERATOR *iter;
	int32_t check_list[HASH_SIZE * 5];

	void SetUp()
	{
		reset_all_fake();
		hash_list = NULL;
		iter = NULL;
		hash_list =
		    create_hash_list(_hash_ftn, _key_cmp_ftn, NULL, HASH_SIZE,
				     sizeof(test_key_t), sizeof(test_data_t));

		memset(check_list, 0, sizeof(int32_t) * HASH_SIZE * 5);
	}

	void TearDown()
	{
		if (iter)
			destroy_hashlist_iter(iter);
		if (hash_list)
			destroy_hash_list(hash_list);
	}
};

TEST_F(next_entryTest, EmptyHashlist)
{
	ASSERT_TRUE(hash_list != NULL);
	iter = init_hashlist_iter(hash_list);
	ASSERT_TRUE(iter != NULL);

	EXPECT_EQ(NULL, iter_begin(iter));
	EXPECT_EQ(ENOENT, errno);
}

TEST_F(next_entryTest, Traverse_UniformHashlist)
{
	int32_t now_key;

	for (int32_t count = 0; count < HASH_SIZE * 5; count++) {
		test_key_t key = {.key = count };
		test_data_t data = {.data = count };

		ASSERT_EQ(0, insert_hash_list_entry(hash_list, &key, &data));
	}

	ASSERT_TRUE(hash_list != NULL);
	iter = init_hashlist_iter(hash_list);
	ASSERT_TRUE(iter != NULL);

	/* Verify */
	while (iter_next(iter)) {
		now_key = ((test_key_t *)(iter->now_key))->key;
		ASSERT_EQ(now_key, ((test_data_t *)(iter->now_data))->data);
		check_list[now_key] = 1;
	}
	ASSERT_EQ(ENOENT, errno);

	for (int32_t count = 0; count < HASH_SIZE * 5; count++)
		ASSERT_EQ(1, check_list[count]);
}

TEST_F(next_entryTest, Traverse_HalfHashlist)
{
	int32_t now_key;

	for (int32_t count = 0; count < HASH_SIZE / 2; count++) {
		test_key_t key = {.key = count };
		test_data_t data = {.data = count };

		ASSERT_EQ(0, insert_hash_list_entry(hash_list, &key, &data));
	}

	ASSERT_TRUE(hash_list != NULL);
	iter = init_hashlist_iter(hash_list);
	ASSERT_TRUE(iter != NULL);

	/* Verify */
	while (iter_next(iter)) {
		now_key = ((test_key_t *)(iter->now_key))->key;
		ASSERT_EQ(now_key, ((test_data_t *)(iter->now_data))->data);
		check_list[now_key] = 1;
	}
	ASSERT_EQ(ENOENT, errno);

	for (int32_t count = 0; count < HASH_SIZE / 2; count++)
		ASSERT_EQ(1, check_list[count]);
}
/**
 * End unittest for next_entry()
 */

/**
 * Unittest for begin_entry()
 */
class begin_entryTest : public ::testing::Test {
protected:
	HASH_LIST *hash_list;
	HASH_LIST_ITERATOR *iter;
	int32_t check_list[HASH_SIZE * 5];

	void SetUp()
	{
		reset_all_fake();
		hash_list = NULL;
		iter = NULL;
		hash_list =
		    create_hash_list(_hash_ftn, _key_cmp_ftn, NULL, HASH_SIZE,
				     sizeof(test_key_t), sizeof(test_data_t));
		memset(check_list, 0, sizeof(int32_t) * HASH_SIZE * 5);
	}

	void TearDown()
	{
		if (iter)
			destroy_hashlist_iter(iter);
		if (hash_list)
			destroy_hash_list(hash_list);
	}
};

TEST_F(begin_entryTest, EmptyHashList)
{
	ASSERT_TRUE(hash_list != NULL);
	iter = init_hashlist_iter(hash_list);
	ASSERT_TRUE(iter != NULL);

	EXPECT_EQ(NULL, iter_begin(iter));
	EXPECT_EQ(ENOENT, errno);
}

TEST_F(begin_entryTest, JumpToBegining)
{
	int32_t now_key;

	for (int32_t count = 0; count < HASH_SIZE * 5; count++) {
		test_key_t key = {.key = count };
		test_data_t data = {.data = count };

		ASSERT_EQ(0, insert_hash_list_entry(hash_list, &key, &data));
	}

	ASSERT_TRUE(hash_list != NULL);
	iter = init_hashlist_iter(hash_list);
	ASSERT_TRUE(iter != NULL);

	ASSERT_TRUE(NULL != iter_next(iter));
	ASSERT_TRUE(NULL != iter_next(iter));
	ASSERT_TRUE(NULL != iter_next(iter));

	/* Jump to first one */
	ASSERT_TRUE(NULL != iter_begin(iter));

	/* Verify */
	do {
		now_key = ((test_key_t *)(iter->now_key))->key;
		ASSERT_EQ(now_key, ((test_data_t *)(iter->now_data))->data);
		check_list[now_key] = 1;
	} while (iter_next(iter));
	ASSERT_EQ(ENOENT, errno);

	for (int32_t count = 0; count < HASH_SIZE * 5; count++)
		ASSERT_EQ(1, check_list[count]);
}
/**
 * End unittest for begin_entry()
 */

/**
 * Unittest for init_dir_iter()
 */
class init_dir_iterTest : public ::testing::Test {
protected:
	FILE *fptr;
	DIR_ENTRY_ITERATOR *iter;

	void SetUp()
	{
		reset_all_fake();
		iter = NULL;
		fptr = fopen("testpatterns/test_dir_meta__entry_testi", "r");
	}

	void TearDown()
	{
		if (fptr)
			fclose(fptr);
		if (iter)
			destroy_dir_iter(iter);
	}
};

TEST_F(init_dir_iterTest, InitSuccess)
{
	ASSERT_TRUE(fptr != NULL);

	/* Run */
	iter = init_dir_iter(fptr);
	ASSERT_TRUE(iter != NULL);

	/* Verify */
	EXPECT_EQ(iter->fptr, fptr);
	EXPECT_EQ(iter->base.begin, begin_dir_entry);
	EXPECT_EQ(iter->base.next, next_dir_entry);
	EXPECT_EQ(iter->now_dirpage_pos, -1);
	EXPECT_EQ(iter->now_entry_idx, -1);
	EXPECT_EQ(iter->now_entry, NULL);
}

TEST_F(init_dir_iterTest, InitFail_NOMEM)
{
	ASSERT_TRUE(fptr != NULL);
	calloc_fake.custom_fake = fake_calloc;
	calloc_success = 0;

	/* Run */
	iter = init_dir_iter(fptr);
	ASSERT_EQ(NULL, iter);
	ASSERT_EQ(ENOMEM, errno);
}

TEST_F(init_dir_iterTest, InitFail_fseekFail)
{
	ASSERT_TRUE(fptr != NULL);
	fseek_fake.custom_fake = fake_fseek;
	fseek_success = 0;

	/* Run */
	iter = init_dir_iter(fptr);
	ASSERT_EQ(NULL, iter);
	ASSERT_EQ(EIO, errno);
}
/**
 * End unittest for init_dir_iter()
 */

/**
 * Unittest for next_dir_entry()
 */
class next_dir_entryTest : public ::testing::Test {
protected:
	FILE *fptr;
	DIR_ENTRY_ITERATOR *iter;
	int32_t check_list[10000];


	void SetUp()
	{
		reset_all_fake();
		iter = NULL;
		fptr = fopen("testpatterns/test_dir_meta__entry_testi", "r");
		memset(check_list, 0, sizeof(int32_t) * 10000);
	}

	void TearDown()
	{
		if (fptr)
			fclose(fptr);
		if (iter)
			destroy_dir_iter(iter);
	}
};

TEST_F(next_dir_entryTest, fseekFail)
{
	int32_t ret, number;
	int32_t entry_count;

	ASSERT_TRUE(fptr != NULL);
	iter = init_dir_iter(fptr);
	ASSERT_TRUE(iter != NULL);

	fseek_fake.custom_fake = fake_fseek;
	fseek_success = 0;
	/* Run */
	EXPECT_TRUE(iter_next(iter) == NULL);
	EXPECT_EQ(EIO, errno);
}

TEST_F(next_dir_entryTest, TraverseAllSuccess)
{
	int32_t ret, number;
	int32_t entry_count;

	ASSERT_TRUE(fptr != NULL);
	iter = init_dir_iter(fptr);
	ASSERT_TRUE(iter != NULL);

	/* Run */
	entry_count = 0;
	while (iter_next(iter)) {
		ret = sscanf(iter->now_entry->d_name, "test%d", &number);
		if (ret == 0)
			continue;
		ASSERT_TRUE(number < 10000);
		check_list[number] = 1;
		entry_count++;
	}
	EXPECT_EQ(ENOENT, errno);
	EXPECT_TRUE(iter_next(iter) == NULL);
	EXPECT_EQ(ENOENT, errno);

	/* Verify */
	ASSERT_EQ(1000, entry_count);
	for (int32_t count = 0; count < entry_count; count++)
		ASSERT_EQ(1, check_list[count]);
}
/**
 * End unittest for next_dir_entry()
 */

/**
 * Unittest for begin_dir_entry()
 */
class begin_dir_entryTest : public ::testing::Test {
protected:
	FILE *fptr;
	DIR_ENTRY_ITERATOR *iter;
	int32_t check_list[10000];


	void SetUp()
	{
		reset_all_fake();
		iter = NULL;
		fptr = fopen("testpatterns/test_dir_meta__entry_testi", "r");
		memset(check_list, 0, sizeof(int32_t) * 10000);
	}

	void TearDown()
	{
		if (fptr)
			fclose(fptr);
		if (iter)
			destroy_dir_iter(iter);
	}
};

TEST_F(begin_dir_entryTest, JumpToFirstSuccess)
{
	int32_t ret, number;
	int32_t entry_count;

	ASSERT_TRUE(fptr != NULL);
	iter = init_dir_iter(fptr);
	ASSERT_TRUE(iter != NULL);

	/* Run to last one */
	while (iter_next(iter)) {}
	EXPECT_EQ(ENOENT, errno);

	/* Jump to first one */
	ASSERT_TRUE(iter_begin(iter) != NULL);

	/* Run */
	entry_count = 0;
	do {
		ret = sscanf(iter->now_entry->d_name, "test%d", &number);
		if (ret == 0)
			continue;
		ASSERT_TRUE(number < 10000);
		check_list[number] = 1;
		entry_count++;
	} while (iter_next(iter));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_TRUE(iter_next(iter) == NULL);
	EXPECT_EQ(ENOENT, errno);

	/* Verify */
	ASSERT_EQ(1000, entry_count);
	for (int32_t count = 0; count < entry_count; count++)
		ASSERT_EQ(1, check_list[count]);
}

/**
 * End unittest for begin_dir_entry()
 */

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
#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>

extern "C" {
#include "dedup_table.h"
}

int test_elements = 70000;

class testEnvironment : public ::testing::Environment {
	public:
		virtual void SetUp() {
			if (access("testpatterns/ddt", F_OK) == -1)
				mkdir("testpatterns/ddt", 0700);
		}

		virtual void TearDown() {
			printf("Removing temp files...\n");
			unlink("testpatterns/ddt/test_ddt_meta");
			unlink("testpatterns/ddt/ddt_meta_00");
			unlink("testpatterns/ddt/for_delete_test");
			unlink("testpatterns/ddt/for_increase_refcount_test");
			unlink("testpatterns/ddt/for_insert_test");
			unlink("testpatterns/ddt/for_mock_test");
			unlink("testpatterns/ddt/for_search_test");
			printf("Finished\n");
		}
};

::testing::Environment* const dedup_table_env = ::testing::AddGlobalTestEnvironment(new testEnvironment);

void _get_mock_hash(unsigned char output_hash[], int seed) {

	int hash_length = SHA256_DIGEST_LENGTH;
	int size_to_cmp = 0;
	int count;
	int mod;

	/* Clear output hash */
	memset(output_hash, 0, hash_length);

	size_to_cmp = seed;

	count = 1;
	while (1) {
		mod = size_to_cmp % 256;
		size_to_cmp = size_to_cmp / 256;
		memcpy(&(output_hash[hash_length - count]), &mod, 1);
		memcpy(&(output_hash[hash_length - (count + 1)]), &size_to_cmp, 1);
		if (size_to_cmp < 256)
			break;
		count += 1;
	}
}

/* Base class to prepare tree for test */
class prepare_tree : public ::testing::Test {
        protected:
		char test_tree_path[400];
		unsigned char testdata[OBJID_LENGTH] = {0};
		FILE *fptr;
		int fd;
		DDT_BTREE_NODE tempnode;
		DDT_BTREE_META tempmeta;

		virtual void get_tree_name(char path[]){
			sprintf(path, "%s", "testpatterns/ddt/tree_for_test");
		}

                virtual void SetUp()
                {

			get_tree_name(test_tree_path);

			/* Prepare tree */
			initialize_ddt_meta(test_tree_path);

			/* Insert test data */
			fptr = fopen(test_tree_path, "r+");
			fd = fileno(fptr);

			memset(&tempnode, 0, sizeof(DDT_BTREE_NODE));
			memset(&tempmeta, 0, sizeof(DDT_BTREE_META));
			pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);

			/* Elements will be inserted to tree
			 *
			 *   0000...00000
			 *   0000...00001
			 *   0000...00002
			 *       .
			 *       .
			 *   0000...00008
			 *   0000...00009
			 *       .
			 *       .
			 */
			for (int i = 0; i < test_elements; i++) {
				pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
				pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

				_get_mock_hash(testdata, i);
				memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));
				//for (int tmp_idx = 0; tmp_idx < 32; ++tmp_idx) {
				//	printf("%02x", testdata[tmp_idx]);
				//}
				//printf("\n");
				insert_ddt_btree(testdata, 0, &tempnode, fd, &tempmeta);
			}

			// Check if insert OK
			//pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
			//pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);
			//traverse_ddt_btree(&tempnode, fd);

			fclose(fptr);
                }

                virtual void TearDown()
                {
                }
};

/* Test for initialize_ddt_meta */
TEST(initialize_ddt_metaTest, initOK) {

	char metapath[] = "testpatterns/ddt/test_ddt_meta";
	FILE *fptr;
	DDT_BTREE_META tempmeta;

	memset(&tempmeta, 0, sizeof(DDT_BTREE_META));

	EXPECT_EQ(0, initialize_ddt_meta(metapath));

	fptr = fopen(metapath, "r");

	fread(&tempmeta, sizeof(DDT_BTREE_META), 1, fptr);

	/* Compare arguments of tree meta */
	EXPECT_EQ(0, tempmeta.tree_root);
	EXPECT_EQ(0, tempmeta.total_el);
	EXPECT_EQ(0, tempmeta.node_gc_list);

	fclose(fptr);
}

TEST(get_ddt_btree_metaTest, getOK) {

	unsigned char key[1] = {0};
	FILE *fptr;
	DDT_BTREE_NODE tempnode;
	DDT_BTREE_META tempmeta;

	memset(&tempnode, 0, sizeof(DDT_BTREE_NODE));
	memset(&tempmeta, 0, sizeof(DDT_BTREE_META));

	fptr = get_ddt_btree_meta(key, &tempnode, &tempmeta);
	EXPECT_EQ(0, tempmeta.tree_root);
	EXPECT_EQ(0, tempmeta.total_el);
	EXPECT_EQ(0, tempmeta.node_gc_list);

	fclose(fptr);
}

/*
        Unittest of search_ddt_btree()
 */
class search_ddt_btreeTest : public prepare_tree {
	protected:
		virtual void get_tree_name(char path[]){
			sprintf(path, "%s", "testpatterns/ddt/for_search_test");
		}
};

TEST_F(search_ddt_btreeTest, searchOK) {

	int s_idx, r_idx;
	DDT_BTREE_NODE t_node, r_node;
	DDT_BTREE_META tempmeta;

	memset(&r_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&t_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&tempmeta, 0, sizeof(DDT_BTREE_META));

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	/* Start to search */
	for (s_idx = 0; s_idx < test_elements; s_idx++) {
		pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
		pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

		_get_mock_hash(testdata, s_idx);
		/* Compare return value */
		ASSERT_EQ(0, search_ddt_btree(testdata, &t_node, fd, &r_node, &r_idx));
		/* Compare key we found */
		EXPECT_EQ(0, memcmp(testdata, r_node.ddt_btree_el[r_idx].obj_id, SHA256_DIGEST_LENGTH));
	}

	fclose(fptr);
}

TEST_F(search_ddt_btreeTest, search_non_existed_el) {

	int s_idx, r_idx;
	DDT_BTREE_NODE t_node, r_node;
	DDT_BTREE_META tempmeta;

	memset(&r_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&t_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&tempmeta, 0, sizeof(DDT_BTREE_META));

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	/* Start to search key - 0000..00011 */
	s_idx = test_elements + 1;
	_get_mock_hash(testdata, s_idx);
	memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));
	EXPECT_EQ(1, search_ddt_btree(testdata, &t_node, fd, &r_node, &r_idx));
}

TEST_F(search_ddt_btreeTest, search_empty_node) {

	int s_idx, r_idx;
	DDT_BTREE_NODE t_node, r_node;
	DDT_BTREE_META tempmeta;

	memset(&r_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&t_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&tempmeta, 0, sizeof(DDT_BTREE_META));

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	/* Force t_node has no elements */
	t_node.num_el = 0;

	/* Start to search */
	s_idx = 0;
	_get_mock_hash(testdata, s_idx);
	memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));
	EXPECT_EQ(-1, search_ddt_btree(testdata, &t_node, fd, &r_node, &r_idx));

	fclose(fptr);
}

TEST_F(search_ddt_btreeTest, goto_error_handler) {

	int s_idx, r_idx;
	int ret_val, has_err;
	DDT_BTREE_NODE t_node, r_node;
	DDT_BTREE_META tempmeta;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	/* Force to close meta file */
	fclose(fptr);

	ret_val = search_ddt_btree(testdata, &t_node, fd, &r_node, &r_idx);
	if (ret_val < 0)
		has_err = 1;
	else
		has_err = 0;

	EXPECT_EQ(1, has_err);
}
/*
        End unittest of search_ddt_btree()
 */

/*
        Unittest of insert_ddt_btree()
 */
class insert_ddt_btreeTest : public ::testing::Test {
        protected:
		char *test_tree_path = "testpatterns/ddt/for_insert_test";
		FILE *fptr;
		int fd;
		DDT_BTREE_NODE tempnode;
		DDT_BTREE_META tempmeta;

                virtual void SetUp()
                {
			/* Prepare tree */
			initialize_ddt_meta(test_tree_path);

			/* Insert test data */
			fptr = fopen(test_tree_path, "r+");
			fd = fileno(fptr);

			memset(&tempnode, 0, sizeof(DDT_BTREE_NODE));
			memset(&tempmeta, 0, sizeof(DDT_BTREE_META));
			pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);

			fclose(fptr);
                }

                virtual void TearDown()
                {
                }
};

TEST_F(insert_ddt_btreeTest, insertOK) {

	unsigned char testdata[OBJID_LENGTH] = {0};

	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	/* Total insert 10 elements */
	for (int i = 0; i < test_elements; i++) {
		_get_mock_hash(testdata, i);
		memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));
		EXPECT_EQ(0, insert_ddt_btree(testdata, 0, &tempnode, fd, &tempmeta));
	}

	/* Reload btree meta */
	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	EXPECT_EQ(test_elements, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(insert_ddt_btreeTest, goto_error_handler) {

	unsigned char testdata[OBJID_LENGTH] = {0};
	int ret_val, has_err;

	/* Total insert 10 elements */
	for (int i = 9; i >= 0; i--) {
		_get_mock_hash(testdata, i);
		memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));
		ret_val = insert_ddt_btree(testdata, 0, &tempnode, fd, &tempmeta);
		if (ret_val < 0)
			has_err = 1;
		else
			has_err = 0;

		EXPECT_EQ(1, has_err);
	}
}
/*
        End unittest of insert_ddt_btree()
 */

/*
        Unittest of delete_ddt_btree()
 */
class delete_ddt_btreeTest : public prepare_tree {
	protected:
		virtual void get_tree_name(char path[]){
			sprintf(path, "%s", "testpatterns/ddt/for_delete_test");
		}
};

TEST_F(delete_ddt_btreeTest, deleteOK) {

	unsigned char testdata[OBJID_LENGTH] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	_get_mock_hash(testdata, test_elements - 1);
	memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));

	EXPECT_EQ(0, delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete));
	EXPECT_EQ(test_elements - 1, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, delete_internal_element) {

	int ret_val, has_err;
	int force_delete = 1;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(0, delete_ddt_btree(tempnode.ddt_btree_el[0].obj_id,
				&tempnode, fd, &tempmeta, force_delete));
	EXPECT_EQ(test_elements - 1, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, delete_all_elements) {

	unsigned char testdata[OBJID_LENGTH] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(test_elements, tempmeta.total_el);

	for (int i = 0; i < test_elements; i++) {
		pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
		pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

		_get_mock_hash(testdata, i);
		memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));
		EXPECT_EQ(0, delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete));
	}

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(0, tempmeta.total_el);
	EXPECT_EQ(0, tempnode.num_el);

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, delete_non_existed_el) {

	unsigned char testdata[OBJID_LENGTH] = {0};
	int key;
	int ret_val, has_err;
	int force_delete = 1;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(test_elements, tempmeta.total_el);

	key = test_elements + 1;
	_get_mock_hash(testdata, key);
	memset(&(testdata[SHA256_DIGEST_LENGTH]), 0, (BYTES_TO_CHECK * 2));
	EXPECT_EQ(2, delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete));

	EXPECT_EQ(test_elements, tempmeta.total_el);

	fclose(fptr);
}

/*
 * Decrease the refcount of a element which has the
 * value of refcount is 1. In this case, this element
 * will be removed from tree.
 */
TEST_F(delete_ddt_btreeTest, decrease_with_refcount_1) {

	int ret_val, has_err;
	int force_delete = 0;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(0, delete_ddt_btree(tempnode.ddt_btree_el[0].obj_id,
				&tempnode, fd, &tempmeta, force_delete));
	EXPECT_EQ(test_elements - 1, tempmeta.total_el);

	fclose(fptr);
}

/*
 * Decrease the refcount of a element which has the
 * value of refcount is larger than 1.
 */
TEST_F(delete_ddt_btreeTest, decrease_with_refcount_2) {

	int ret_val, has_err;
	int force_delete = 0;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	/* Need to increase the refcount manually */
	tempnode.ddt_btree_el[0].refcount += 1;

	EXPECT_EQ(1, delete_ddt_btree(tempnode.ddt_btree_el[0].obj_id,
				&tempnode, fd, &tempmeta, force_delete));
	EXPECT_EQ(test_elements, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, empty_node) {

	unsigned char testdata[OBJID_LENGTH] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	/* Force this node has no elements */
	tempnode.num_el = 0;

	EXPECT_EQ(-1, delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete));

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, goto_error_handler) {

	unsigned char testdata[OBJID_LENGTH] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	fclose(fptr);

	ret_val = delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete);
	if (ret_val < 0)
		has_err = 1;
	else
		has_err = 0;

	EXPECT_EQ(1, has_err);
}
/*
        End unittest of delete_ddt_btree()
 */

/*
        Unittest of increase_ddt_el_refcount()
 */
class increase_ddt_el_refcountTest : public prepare_tree {
	protected:
		virtual void get_tree_name(char path[]){
			sprintf(path, "%s", "testpatterns/ddt/for_increase_refcount_test");
		}
};

TEST_F(increase_ddt_el_refcountTest, increaseOK) {

	int old_refcount;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	old_refcount = tempnode.ddt_btree_el[0].refcount;

	EXPECT_EQ(0, increase_ddt_el_refcount(&tempnode, 0, fd));

	EXPECT_EQ(old_refcount + 1, tempnode.ddt_btree_el[0].refcount);

	fclose(fptr);
}

TEST_F(increase_ddt_el_refcountTest, wrong_search_index) {

	int old_refcount;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	old_refcount = tempnode.ddt_btree_el[0].refcount;

	EXPECT_EQ(-1, increase_ddt_el_refcount(&tempnode, 9999, fd));

	EXPECT_EQ(old_refcount, tempnode.ddt_btree_el[0].refcount);

	fclose(fptr);
}

TEST_F(increase_ddt_el_refcountTest, goto_error_handler) {

	int ret_val, has_err;

	/* Get tree file handler */
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	/* Force to close meta file */
	fclose(fptr);

	ret_val = increase_ddt_el_refcount(&tempnode, 0, fd);
	if (ret_val < 0)
		has_err = 1;
	else
		has_err = 0;

	EXPECT_EQ(1, has_err);
}
/*
        Unittest of increase_ddt_el_refcount()
 */

TEST(get_obj_idTest, computeOK) {

	char *data_path = "testpatterns/testdata.1M";
	/* Actual hash is computed by openssl cli */
	char actual_hash[65] = "ac596f8d08e48824ceb4bdcc1085ca18f977ef166e67280808400d818d49616e";
	char hash_str[65];
	unsigned char hash[SHA256_DIGEST_LENGTH];
	/* To verify bytes of content of test data */
	unsigned char actual_start_bytes[BYTES_TO_CHECK];
	unsigned char actual_end_bytes[BYTES_TO_CHECK];
	unsigned char start_bytes[BYTES_TO_CHECK];
	unsigned char end_bytes[BYTES_TO_CHECK];
	off_t size;
	int i;

	EXPECT_EQ(0, get_obj_id(data_path, hash, start_bytes, end_bytes, &size));

	/* Convert to string */
	for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
		sprintf(hash_str + (i * 2), "%02x", hash[i]);
	hash_str[64] = 0;

	/* Make sure hash computed is correct */
	EXPECT_STREQ(actual_hash, hash_str);
	EXPECT_EQ(1048576, size);

	FILE *fptr = fopen(data_path, "rb");
	fread(actual_start_bytes, BYTES_TO_CHECK, 1, fptr);
	fseek(fptr, -BYTES_TO_CHECK, SEEK_END);
	fread(actual_end_bytes, BYTES_TO_CHECK, 1, fptr);

	EXPECT_EQ(0, memcmp(start_bytes, actual_start_bytes, BYTES_TO_CHECK));
	EXPECT_EQ(0, memcmp(end_bytes, actual_end_bytes, BYTES_TO_CHECK));
}

TEST(get_obj_idTest, non_existed_path) {

	char *data_path = "testpatterns/data_nonexisted";
	unsigned char hash[SHA256_DIGEST_LENGTH];
	unsigned char start_bytes[BYTES_TO_CHECK];
	unsigned char end_bytes[BYTES_TO_CHECK];
	off_t size;

	EXPECT_EQ(-1, get_obj_id(data_path, hash, start_bytes, end_bytes, &size));
}

//class mockTest : public prepare_tree {
//	protected:
//		virtual void get_tree_name(char path[]){
//			sprintf(path, "%s", "testpatterns/ddt/for_mock_test");
//		}
//};
//
//TEST_F(mockTest, searchOK) {
//	EXPECT_EQ(0, 0);
//}

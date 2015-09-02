#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>

extern "C" {
#include "dedup_table.h"
}

int test_elements = 10;

// Base class to prepare tree for test
class prepare_tree : public ::testing::Test {
        protected:
		char test_tree_path[400];
		unsigned char testdata[32] = {0};
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

			// Prepare tree
			initialize_ddt_meta(test_tree_path);

			// Insert test data
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
				memcpy(&(testdata[31]), &i, 1);
				insert_ddt_btree(testdata, &tempnode, fd, &tempmeta);
			}

			// Check if insert OK
			//traverse_ddt_btree(&tempnode, fd);

			fclose(fptr);
                }

                virtual void TearDown()
                {
                }
};

// Test for initialize_ddt_meta
TEST(initialize_ddt_metaTest, initOK) {

	char metapath[] = "testpatterns/ddt/test_ddt_meta";
	FILE *fptr;
	DDT_BTREE_META tempmeta;

	memset(&tempmeta, 0, sizeof(DDT_BTREE_META));

	EXPECT_EQ(0, initialize_ddt_meta(metapath));

	fptr = fopen(metapath, "r");

	fread(&tempmeta, sizeof(DDT_BTREE_META), 1, fptr);

	// Compare arguments of tree meta
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

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	// Start to search
	for (s_idx = 0; s_idx < test_elements; s_idx++) {
		memcpy(&(testdata[31]), &s_idx, 1);
		// Compare return value
		EXPECT_EQ(0, search_ddt_btree(testdata, &t_node, fd, &r_node, &r_idx));
		// Compare key we found
		EXPECT_EQ(0, memcmp(testdata, r_node.ddt_btree_el[r_idx].obj_id, 32));
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

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	// Start to search key - 0000..00011
	s_idx = test_elements + 1;
	memcpy(&(testdata[31]), &s_idx, 1);
	EXPECT_EQ(1, search_ddt_btree(testdata, &t_node, fd, &r_node, &r_idx));
}

TEST_F(search_ddt_btreeTest, search_empty_node) {

	int s_idx, r_idx;
	DDT_BTREE_NODE t_node, r_node;
	DDT_BTREE_META tempmeta;

	memset(&r_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&t_node, 0, sizeof(DDT_BTREE_NODE));
	memset(&tempmeta, 0, sizeof(DDT_BTREE_META));

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	// Force t_node has no elements
	t_node.num_el = 0;

	// Start to search
	s_idx = 0;
	memcpy(&(testdata[31]), &s_idx, 1);
	EXPECT_EQ(-1, search_ddt_btree(testdata, &t_node, fd, &r_node, &r_idx));

	fclose(fptr);
}

TEST_F(search_ddt_btreeTest, goto_error_handler) {

	int s_idx, r_idx;
	int ret_val, has_err;
	DDT_BTREE_NODE t_node, r_node;
	DDT_BTREE_META tempmeta;

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &t_node, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	// Force to close meta file
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
			// Prepare tree
			initialize_ddt_meta(test_tree_path);

			// Insert test data
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

	unsigned char testdata[32] = {0};

	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	// Total insert 10 elements
	for (int i = 0; i < test_elements; i++) {
		memcpy(&(testdata[31]), &i, 1);
		EXPECT_EQ(0, insert_ddt_btree(testdata, &tempnode, fd, &tempmeta));
	}

	// Reload btree meta
	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	EXPECT_EQ(10, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(insert_ddt_btreeTest, goto_error_handler) {

	unsigned char testdata[32] = {0};
	int ret_val, has_err;

	// Total insert 10 elements
	for (int i = 9; i >= 0; i--) {
		memcpy(&(testdata[31]), &i, 1);
		ret_val = insert_ddt_btree(testdata, &tempnode, fd, &tempmeta);
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

	unsigned char testdata[32] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(0, delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete));
	EXPECT_EQ(test_elements - 1, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, delete_internal_element) {

	int ret_val, has_err;
	int force_delete = 1;

	// Get tree file handler
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

	unsigned char testdata[32] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(10, tempmeta.total_el);

	for (int i = 0; i < test_elements; i++) {
		memcpy(&(testdata[31]), &i, 1);
		EXPECT_EQ(0, delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete));
	}

	EXPECT_EQ(0, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, delete_non_existed_el) {

	unsigned char testdata[32] = {0};
	int key;
	int ret_val, has_err;
	int force_delete = 1;

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	EXPECT_EQ(test_elements, tempmeta.total_el);

	key = test_elements + 1;
	memcpy(&(testdata[31]), &key, 1);
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

	// Get tree file handler
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

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	// Need to increase the refcount manually
	tempnode.ddt_btree_el[0].refcount += 1;

	EXPECT_EQ(1, delete_ddt_btree(tempnode.ddt_btree_el[0].obj_id,
				&tempnode, fd, &tempmeta, force_delete));
	EXPECT_EQ(test_elements, tempmeta.total_el);

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, empty_node) {

	unsigned char testdata[32] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	// Force this node has no elements
	tempnode.num_el = 0;

	EXPECT_EQ(-1, delete_ddt_btree(testdata, &tempnode, fd, &tempmeta, force_delete));

	fclose(fptr);
}

TEST_F(delete_ddt_btreeTest, goto_error_handler) {

	unsigned char testdata[32] = {0};
	int ret_val, has_err;
	int force_delete = 1;

	// Get tree file handler
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

	// Get tree file handler
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

	// Get tree file handler
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

	// Get tree file handler
	fptr = fopen(test_tree_path, "r+");
	fd = fileno(fptr);

	pread(fd, &tempmeta, sizeof(DDT_BTREE_META), 0);
	pread(fd, &tempnode, sizeof(DDT_BTREE_NODE), tempmeta.tree_root);

	// Force to close meta file
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

TEST(compute_hashTest, computeOK) {

	char *data_path = "testpatterns/testdata.1M";
	// Actual hash is computed by openssl cli
	char actual_hash[65] = "ac596f8d08e48824ceb4bdcc1085ca18f977ef166e67280808400d818d49616e";
	char hash_str[65];
	unsigned char hash[32];

	EXPECT_EQ(0, compute_hash(data_path, hash));

	// Convert to string
	hash_to_string(hash, hash_str);

	// Make sure hash computed is correct
	EXPECT_STREQ(actual_hash, hash_str);
}

TEST(compute_hashTest, non_existed_path) {

	char *data_path = "testpatterns/data_nonexisted";
	unsigned char hash[32];

	EXPECT_EQ(-1, compute_hash(data_path, hash));
}


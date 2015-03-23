#include "gtest/gtest.h"
#include "cstdlib"
extern "C" {
#include "dir_entry_btree.h"
#include "fuseop.h"
}

static inline int compare(const void *a, const void *b)
{
	return strcmp(((DIR_ENTRY *)a)->d_name, ((DIR_ENTRY *)b)->d_name);
}

/*
	Unittest of dentry_binary_search()
 */

class dentry_binary_searchTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			entry_array = NULL;
		}
		virtual void TearDown()
		{
			if (entry_array != NULL)
				free(entry_array);
		}
		void generate_mock_data(int num)
		{
			if (entry_array != NULL)
				free(entry_array);
			entry_array = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY) * num);
			for (int i = 0 ; i < num ; i++) {
				char test_filename[30];
				sprintf(test_filename, "test_filename%d", i);
				strcpy(entry_array[i].d_name, test_filename);
			}
			qsort(entry_array, num, sizeof(DIR_ENTRY), compare);

		}
		int linear_search(int num, DIR_ENTRY *entry)
		{
			for (int i = 0 ; i < num ; i++) 
				if (compare(entry_array + i, entry) >= 0)
					return i;
			return num;
		}
		DIR_ENTRY *entry_array;
};

TEST_F(dentry_binary_searchTest, EntryNotFound)
{
	int num_entry = 500;
	/* Mock data */
	generate_mock_data(num_entry);
	/* Test */
	for (int times = 0 ; times < 100 ; times++) {
		DIR_ENTRY entry;
		int index;
		sprintf(entry.d_name, "test_filename%d123", times);
		ASSERT_EQ(-1, dentry_binary_search(entry_array, num_entry, &entry, &index));
		EXPECT_EQ(linear_search(num_entry, &entry), index);
	}
}

TEST_F(dentry_binary_searchTest, BoundaryTest)
{	
	DIR_ENTRY entry;
	int index;
	int num_entry = 500;
	/* Mock data */
	generate_mock_data(num_entry);
	/* Test index 0 */
	strcpy(entry.d_name, "test");
	ASSERT_EQ(-1, dentry_binary_search(entry_array, num_entry, &entry, &index));
	EXPECT_EQ(0, index);
	/* Test last index */
	strcpy(entry.d_name, "test_filename9999999");
	ASSERT_EQ(-1, dentry_binary_search(entry_array, num_entry, &entry, &index));
	EXPECT_EQ(500, index);
}

TEST_F(dentry_binary_searchTest, FindEntrySuccess)
{
	int num_entry = 5000;
	/* Mock data */
	generate_mock_data(num_entry);
	/* Test */
	for (int times = 0 ; times < 100 ; times++) {
		DIR_ENTRY entry;
		int index;
		int ans;
		sprintf(entry.d_name, "test_filename%d", times * 23);
		ans = linear_search(num_entry, &entry);
		ASSERT_EQ(ans, dentry_binary_search(entry_array, num_entry, &entry, &index));
	}
}


/*
	End of unittest of dentry_binary_search()
 */

/*
	Unittest of insert_dir_entry_btree()
 */

#define  _REDIRECT_STDOUT_TO_FILE_(reserved_stdout, filename) \
	reserved_stdout = dup(fileno(stdout)); \
	FILE *stdout_file = fopen(filename, "a+"); \
	setbuf(stdout_file, NULL); \
	dup2(fileno(stdout_file), fileno(stdout));


#define _RESTORE_STDOUT_(reserved_stdout, filename) \
	dup2(reserved_stdout, fileno(stdout)); \
	unlink(filename);

class insert_dir_entry_btreeTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			struct stat dir_stat;
			DIR_META_TYPE meta;
			DIR_ENTRY_PAGE node;
			
			memset(&dir_stat, 0, sizeof(struct stat));
			memset(&meta, 0, sizeof(DIR_META_TYPE));
			memset(&node, 0, sizeof(DIR_ENTRY_PAGE));
			overflow_median = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
			overflow_new_pos = (long long *)malloc(sizeof(long long));
			// root entry
			node.dir_entries[0].d_ino = 1;
			strcpy(node.dir_entries[0].d_name, ".");
			node.dir_entries[0].d_type = D_ISDIR;
			
			node.dir_entries[1].d_ino = 0;
			strcpy(node.dir_entries[1].d_name, "..");
			node.dir_entries[1].d_type = D_ISDIR;

			node.num_entries = 2;
			node.this_page_pos = 
				sizeof(struct stat) + sizeof(DIR_META_TYPE);
			// meta
			meta.root_entry_page = 
				sizeof(struct stat) + sizeof(DIR_META_TYPE);
			meta.tree_walk_list_head = 
				sizeof(struct stat) + sizeof(DIR_META_TYPE);
			// open dir meta file
			fptr = fopen("/tmp/test_dir_meta", "wb+");
			fh = fileno(fptr);
			ASSERT_TRUE(fptr != NULL);
			pwrite(fh, &dir_stat, sizeof(struct stat), 0);
			pwrite(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
			pwrite(fh, &node, sizeof(DIR_ENTRY_PAGE), sizeof(struct stat)+sizeof(DIR_META_TYPE));
		}
		virtual void TearDown()
		{
			if (overflow_median != NULL)
				free(overflow_median);
			if (overflow_new_pos != NULL)
				free(overflow_new_pos);
			close(fh);
			unlink("/tmp/test_dir_meta");
		}
		
		/* Generate a new root if old root was splitted. */
		void generate_new_root()
		{
			DIR_META_TYPE meta;
			DIR_ENTRY_PAGE root;
			DIR_ENTRY_PAGE new_root;

			pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
			pread(fh, &root, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
			/* create a new root */
			if (meta.entry_page_gc_list != 0) {
				pread(fh, &new_root, sizeof(DIR_ENTRY_PAGE), meta.entry_page_gc_list);
				new_root.this_page_pos = meta.entry_page_gc_list;
				meta.entry_page_gc_list = new_root.gc_list_next;
			} else {
				memset(&new_root, 0, sizeof(DIR_ENTRY_PAGE));
				fseek(fptr, 0, SEEK_END);
				new_root.this_page_pos = ftell(fptr);
			}
			/* Insert to head of tree_walk_list */
			new_root.gc_list_next = 0;
			new_root.tree_walk_prev = 0;
			new_root.tree_walk_next = meta.tree_walk_list_head;
			bool no_need_rewrite = false;
			if (meta.tree_walk_list_head == root.this_page_pos) {
				root.tree_walk_prev = new_root.this_page_pos;
			} else {
				DIR_ENTRY_PAGE tree_head;
				pread(fh, &tree_head, sizeof(DIR_ENTRY_PAGE), meta.tree_walk_list_head);
				tree_head.tree_walk_prev = new_root.this_page_pos;
				if (tree_head.this_page_pos == *overflow_new_pos) {
					tree_head.parent_page_pos = new_root.this_page_pos;
					no_need_rewrite = true;
				}
				pwrite(fh, &tree_head, sizeof(DIR_ENTRY_PAGE), meta.tree_walk_list_head);
			}
			meta.tree_walk_list_head = new_root.this_page_pos;
			/* Init new root */
			new_root.parent_page_pos = 0;
			new_root.num_entries = 1;
			memset(new_root.child_page_pos, 0, sizeof(long long)*(MAX_DIR_ENTRIES_PER_PAGE+1));
			memcpy(&(new_root.dir_entries[0]), overflow_median, sizeof(DIR_ENTRY));
			// connect parent to 2 children
			new_root.child_page_pos[0] = meta.root_entry_page;
			new_root.child_page_pos[1] = *overflow_new_pos;
			// modify root position im meta
			meta.root_entry_page = new_root.this_page_pos;
			pwrite(fh, &new_root, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
			// connect 2 children to parent
			root.parent_page_pos = new_root.this_page_pos;
			pwrite(fh, &root, sizeof(DIR_ENTRY_PAGE), root.this_page_pos);
			if ( no_need_rewrite == false ) {
				DIR_ENTRY_PAGE splitted_node;
				pread(fh, &splitted_node, sizeof(DIR_ENTRY_PAGE), *overflow_new_pos);
				splitted_node.parent_page_pos = new_root.this_page_pos;
				pwrite(fh, &splitted_node, sizeof(DIR_ENTRY_PAGE), *overflow_new_pos);
			}
			pwrite(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
			std::cerr << "\x1B[33mNew root at position " << meta.root_entry_page << std::endl;
		}
		
		/* A simple function to find a given entry so that we can verify our insertion */
		bool search_entry(DIR_ENTRY *entry)
		{
			DIR_META_TYPE *meta = (DIR_META_TYPE *)malloc(sizeof(DIR_META_TYPE));
			DIR_ENTRY_PAGE *node = (DIR_ENTRY_PAGE *)malloc(sizeof(DIR_ENTRY_PAGE));
			int index;
			bool found;

			pread(fh, meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
			pread(fh, node, sizeof(DIR_ENTRY_PAGE), meta->root_entry_page);
			found = false;
			while (!found) {
				int cmp_ret;
				/* linear search */
				index = -1;
				for (int i = 0 ; i < node->num_entries ; i++) {
					cmp_ret = strcmp(node->dir_entries[i].d_name, entry->d_name);
					if (cmp_ret >= 0) {
						index = i;
						break;
					}
				}
				index = index < 0 ? node->num_entries : index; // index<0 means find nothing
				/* found or go deeper */
				if (cmp_ret == 0) { // Check the last comparison
					found = true;
				} else {
					if (node->child_page_pos[index] == 0) { // leaf node
						found = false;
						break;
					} else { // internal node
						pread(fh, node, sizeof(DIR_ENTRY_PAGE), node->child_page_pos[index]);
					}
				}
			}
			free(meta);
			free(node);
			return found;
		}

		int fh;
		FILE *fptr;
		DIR_ENTRY *overflow_median;
		long long *overflow_new_pos;
		DIR_ENTRY tmp_entries[MAX_DIR_ENTRIES_PER_PAGE + 2];
		long long tmp_child_pos[MAX_DIR_ENTRIES_PER_PAGE + 3];
};

TEST_F(insert_dir_entry_btreeTest, Insert_To_Root_Without_Splitting)
{
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
			
	fseek(fptr, sizeof(struct stat), SEEK_SET);
	fread(&meta, sizeof(DIR_META_TYPE), 1, fptr);
	fread(&root_node, sizeof(DIR_ENTRY_PAGE), 1, fptr);
	overflow_median = NULL;
	overflow_new_pos = NULL;

	/* Test for 97 times since max_entries_num==99 (97 + "." + "..") */
	for (int times = 0 ; times < MAX_DIR_ENTRIES_PER_PAGE - 2 ; times++) {
		DIR_ENTRY *entry = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		sprintf(entry->d_name, "test%d", times);
		entry->d_ino = (times + 5) * 3;
		entry->d_type = D_ISDIR;
		ASSERT_EQ(0, insert_dir_entry_btree(entry, &root_node, fh, 
			overflow_median, overflow_new_pos, &meta, tmp_entries, 
			tmp_child_pos));
		ASSERT_TRUE(overflow_median == NULL);
		ASSERT_TRUE(overflow_new_pos == NULL);
		ASSERT_EQ(times + 2 + 1, root_node.num_entries);
		pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), sizeof(struct stat) + sizeof(DIR_META_TYPE));
		ASSERT_EQ(times + 2 + 1, root_node.num_entries);
		free(entry);
		// ASSERT_EQ(times + 1, meta.total_children); // Why not plus 1 in this function?
	}
}

TEST_F(insert_dir_entry_btreeTest, Insert_Many_Entries_With_Splitting)
{
	int reserved_stdout;
	int num_entries_insert = 30000;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
			
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), sizeof(struct stat) + sizeof(DIR_META_TYPE));

	_REDIRECT_STDOUT_TO_FILE_(reserved_stdout, "/tmp/tmpout");
	/* Insert many entries and verified the robustness */
	for (int times = 0 ; times < num_entries_insert ; times++) {
		int ret;
		DIR_ENTRY *entry = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		sprintf(entry->d_name, "test%d", times);
		ret = insert_dir_entry_btree(entry, &root_node, fh, 
			overflow_median, overflow_new_pos, &meta, tmp_entries, 
			tmp_child_pos);
		ASSERT_NE(-1, ret);
		if ( ret == 1 ) {
			generate_new_root();
			pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
			pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
		}
		free(entry);
	}
	_RESTORE_STDOUT_(reserved_stdout, "/tmp/tmpout");

	/* Check those entry in the b-tree */
	for (int times = 0 ; times < num_entries_insert ; times++) {
		DIR_ENTRY *entry = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		sprintf(entry->d_name, "test%d", times);
		ASSERT_EQ(true, search_entry(entry));
		free(entry);
	}

}

TEST_F(insert_dir_entry_btreeTest, InsertFail_EntryFoundInBtree)
{
	int reserved_stdout;
	int num_entries_insert = 10000;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
			
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), sizeof(struct stat) + sizeof(DIR_META_TYPE));

	_REDIRECT_STDOUT_TO_FILE_(reserved_stdout, "/tmp/tmpout");
	/* Insert many entries */
	for (int times = 0 ; times < num_entries_insert ; times++) {
		int ret;
		DIR_ENTRY *entry = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		sprintf(entry->d_name, "test%d", times);
		ret = insert_dir_entry_btree(entry, &root_node, fh, 
			overflow_median, overflow_new_pos, &meta, tmp_entries, 
			tmp_child_pos);
		ASSERT_NE(-1, ret);
		if ( ret == 1 ) {
			generate_new_root();
			pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
			pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
		}
		free(entry);
	}
	_RESTORE_STDOUT_(reserved_stdout, "/tmp/tmpout");

	/* Check whether it exactly failed to insert entry */
	for (int times = 0 ; times < num_entries_insert ; times++) {
		int ret;
		DIR_ENTRY *entry = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
		sprintf(entry->d_name, "test%d", times);
		ret = insert_dir_entry_btree(entry, &root_node, fh, 
			overflow_median, overflow_new_pos, &meta, tmp_entries, 
			tmp_child_pos);
		ASSERT_EQ(-1, ret);
		free(entry);
	}
}

/*
	End of unittest of insert_dir_entry_btree()
 */

/*
	Unittest of search_dir_entry_btree
 */

/* Derive from insert_dir_entry_btreeTest because we can utilize function
   insert_dir_entry_btree() to generate mock data after insert_dir_entry_btree()
   passing unit testing. */

class BaseClassInsertBtreeEntryIsUsable : public insert_dir_entry_btreeTest {
	protected:
		void init_insert_many_entries(int min_num, int max_num, 
					char *filename_prefix)
		{
			int reserved_stdout;
			DIR_META_TYPE meta;
			DIR_ENTRY_PAGE root_node;
			
			pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
			pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), 
				sizeof(struct stat) + sizeof(DIR_META_TYPE));

			_REDIRECT_STDOUT_TO_FILE_(reserved_stdout, "/tmp/tmpout");
			/* Insert many entries */
			for (int times = min_num ; times < max_num ; times++) {
				int ret;
				DIR_ENTRY *entry = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY));
				sprintf(entry->d_name, "%s%d", filename_prefix, times);
				ret = insert_dir_entry_btree(entry, &root_node, fh, 
					overflow_median, overflow_new_pos, &meta, tmp_entries, 
					tmp_child_pos);
				ASSERT_NE(-1, ret);
				if ( ret == 1 ) {
					generate_new_root();
					pread(fh, &meta, sizeof(DIR_META_TYPE), 
						sizeof(struct stat));
					pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), 
						meta.root_entry_page);
				}
				free(entry);
			}
			_RESTORE_STDOUT_(reserved_stdout, "/tmp/tmpout");
		}
};

class search_dir_entry_btreeTest : public BaseClassInsertBtreeEntryIsUsable {

};

TEST_F(search_dir_entry_btreeTest, SearchEmptyBtree)
{
	int index;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY_PAGE result_node;

	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	
	for (int i = 0 ; i < 5000 ; i++) {
		char filename[50];
		sprintf(filename, "search_file_%d", i);
		index = search_dir_entry_btree(filename, &root_node, 
			fh, &index, &result_node);
		EXPECT_EQ(-1, index);
	}
}

TEST_F(search_dir_entry_btreeTest, EntryNotFound)
{
	int index;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY_PAGE result_node;
	/* Mock data */
	init_insert_many_entries(4000, 6000, "test_file");
	/* Search entries */
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	
	for (int i = 0 ; i < 3000 ; i++) {
		char filename[50];
		sprintf(filename, "test_file_not_found%d", i);
		index = search_dir_entry_btree(filename, &root_node, 
			fh, &index, &result_node);
		EXPECT_EQ(-1, index);
	}
}

TEST_F(search_dir_entry_btreeTest, SearchEntrySuccess)
{
	int index;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY_PAGE result_node;
	/* Mock data */
	init_insert_many_entries(0, 10000, "test_file");
	/* Search entries */
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	
	for (int i = 0 ; i < 7000 ; i++) {
		char filename[50];
		sprintf(filename, "test_file%d", i);
		index = search_dir_entry_btree(filename, &root_node, 
			fh, &index, &result_node);
		EXPECT_TRUE(index >= 0);
	}
	/* Finally check "." and ".." */
	index = search_dir_entry_btree(".", &root_node, 
			fh, &index, &result_node);
	EXPECT_TRUE(index >= 0);
	index = search_dir_entry_btree("..", &root_node, 
			fh, &index, &result_node);
	EXPECT_TRUE(index >= 0);
}

/*
	End of unittest of search_dir_entry_btree()
 */

/*
	Unittest of rebalance_btree()
 */

class rebalance_btreeTest : public BaseClassInsertBtreeEntryIsUsable {
	
};

TEST_F(rebalance_btreeTest, ChildIndexOutofBound)
{
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	/* A mock btree with only one page, which is root page. */
	init_insert_many_entries(0, 10, "test_file");
	/* Test */
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);

	for (int child_index = root_node.num_entries+1 ; 
		child_index < root_node.num_entries+1000 ; child_index++) {
		ASSERT_EQ(-1, rebalance_btree(&root_node, fh, &meta,
			child_index, tmp_entries, tmp_child_pos));
	}
}

TEST_F(rebalance_btreeTest, RebalanceBtreeWithOnlyRootPage)
{
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	/* A mock btree with only one page, which is root. */
	init_insert_many_entries(0, MAX_DIR_ENTRIES_PER_PAGE-2, "test_file");
	/* Root node is leaf node, so it needs not rebalance */
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);

	for (int child_index = 0 ; child_index < root_node.num_entries+1 ; child_index++) {
		ASSERT_EQ(-1, rebalance_btree(&root_node, fh, &meta,
			child_index, tmp_entries, tmp_child_pos));
	}
}

TEST_F(rebalance_btreeTest, CheckCase_NoRebalanceNeeded)
{
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	/* A mock btree with depth = 2. There are root and some its children in btree.
	   Number of entries > MAX_DIR_ENTRIES_PER_PAGE/2 > MIN_DIR_ENTRIES_PER_PAGE 
	   for All the children, so it needs not rebalance. */
	init_insert_many_entries(0, 4000, "test_file");
	/* Leaf node needs not rebalance */
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	
	for (int child_index = 0 ; child_index < root_node.num_entries+1 ; child_index++) {
		ASSERT_EQ(0, rebalance_btree(&root_node, fh, &meta,
			child_index, tmp_entries, tmp_child_pos));
	}
}

TEST_F(rebalance_btreeTest, Merge_Without_NewRoot)
{
	int num_entries = MAX_DIR_ENTRIES_PER_PAGE * (MAX_DIR_ENTRIES_PER_PAGE/3);
	int child_index;
	int num_nodes; // The var is used to check tree_walk & gc_list
	int gc_counter;
	int walk_counter;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY_PAGE now_node;
	DIR_ENTRY *remaining_entries; // It is used to check testing result.
	
	/* A mock btree with depth = 2. For each child, let num of 
	   entries < MIN_DIR_ENTRIES_PER_PAGE. */
	init_insert_many_entries(0, num_entries, "test_file");
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	num_nodes = root_node.num_entries + 1 + 1; // num of pages(nodes)
	num_entries = 0;
	for (int i = 0 ; i < root_node.num_entries+1 ; i++) {
		DIR_ENTRY_PAGE child_node;
		pread(fh, &child_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[i]);
		ASSERT_TRUE(child_node.num_entries > MIN_DIR_ENTRIES_PER_PAGE);
		child_node.num_entries = MIN_DIR_ENTRIES_PER_PAGE-1;
		pwrite(fh, &child_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[i]);
		num_entries += child_node.num_entries;
	}
	/* Aggregate the remaining entries in the tree. They are asserted that 
	   all of them will be still in btree after rebalancing. */
	remaining_entries = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY) * 
			(num_entries + root_node.num_entries));
	num_entries = 0;
	for (int i = 0 ; i < root_node.num_entries + 1 ; i++) {
		DIR_ENTRY_PAGE child_node;
		pread(fh, &child_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[i]);
		memcpy(remaining_entries + num_entries, child_node.dir_entries, 
			sizeof(DIR_ENTRY) * child_node.num_entries);
		num_entries += child_node.num_entries;
	}
	memcpy(remaining_entries + num_entries, root_node.dir_entries, 
		sizeof(DIR_ENTRY) * root_node.num_entries);
	num_entries += root_node.num_entries;
	
	/* Run function to test rebalancing */
	child_index = 0;
	while (child_index < root_node.num_entries + 1) {
		int ret = rebalance_btree(&root_node, fh, &meta, 
			child_index, tmp_entries, tmp_child_pos);
		ASSERT_TRUE(ret == 1 || ret == 0);
		if (ret == 1) { // Merging 
			pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
			child_index = 0;
		} else { // No rebalancing 
			child_index++;
		}
	}
	
	/* Check btree structure:
	   1. For each child, num_entries > MIN_DIR_ENTRIES_PER_PAGE 
	   2. All the remaining_entries are still in the btree
	   3. Garbage collection is done
	   4. Tree walking is ok. */
	
	/* 1. Check num_entries */
	for (int i = 0 ; i < root_node.num_entries + 1 ; i++) {
		DIR_ENTRY_PAGE child_node;
		pread(fh, &child_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[i]);
		ASSERT_TRUE(child_node.num_entries > MIN_DIR_ENTRIES_PER_PAGE);
	}
	/* 2. Check all the remaining_entries */
	for(int i = 0 ; i < num_entries ; i++) 
		ASSERT_EQ(true, search_entry(&remaining_entries[i]));
	/* 3.4. Garbage is correctly collected & tree walking */
	// Count num of gc list
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &now_node, sizeof(DIR_ENTRY_PAGE), meta.entry_page_gc_list);
	gc_counter = 1;
	while (now_node.gc_list_next != 0) {
		pread(fh, &now_node, sizeof(DIR_ENTRY_PAGE), now_node.gc_list_next);
		gc_counter++;
	}
	// Count num of tree walking list
	pread(fh, &now_node, sizeof(DIR_ENTRY_PAGE), meta.tree_walk_list_head);
	walk_counter = 1;
	while (now_node.tree_walk_next != 0) {
		pread(fh, &now_node, sizeof(DIR_ENTRY_PAGE), now_node.tree_walk_next);
		walk_counter++;
	}
	// Check answer
	EXPECT_EQ(root_node.num_entries + 1 + 1, walk_counter);
	EXPECT_EQ(num_nodes, walk_counter + gc_counter);

	/* Free resource */
	free(remaining_entries);
}

TEST_F(rebalance_btreeTest, Merge_With_NewRoot)
{	
	int num_entries = MAX_DIR_ENTRIES_PER_PAGE + 2;
	int gc_counter;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY_PAGE tmp_node;
	
	/* Init mock btree */
	init_insert_many_entries(0, num_entries, "test_file");
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	ASSERT_EQ(1, root_node.num_entries);
	num_entries = 0;
	// left
	pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[0]);
	tmp_node.num_entries = MIN_DIR_ENTRIES_PER_PAGE - 1;
	num_entries += tmp_node.num_entries;
	pwrite(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[0]);
	// right
	pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[1]);
	tmp_node.num_entries = MIN_DIR_ENTRIES_PER_PAGE - 1;
	num_entries += tmp_node.num_entries;
	pwrite(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[1]);
	
	/* Run rebalancing function */
	ASSERT_EQ(2, rebalance_btree(&root_node, fh, &meta, 
		0, tmp_entries, tmp_child_pos));

	/* Check answer */	
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	EXPECT_EQ(num_entries + 1, root_node.num_entries);
	
	pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), meta.entry_page_gc_list);
	gc_counter = 1;
	while (tmp_node.gc_list_next != 0) {
		pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), tmp_node.gc_list_next);
		gc_counter++;
	}
	ASSERT_EQ(2, gc_counter);
}

TEST_F(rebalance_btreeTest, NoMerge_SplitInto2Pages)
{
	int num_entries = MAX_DIR_ENTRIES_PER_PAGE + MAX_DIR_ENTRIES_PER_PAGE / 4;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY_PAGE tmp_node;
	DIR_ENTRY *remaining_entries;
	
	/* Init mock btree */
	remaining_entries = (DIR_ENTRY *)malloc(sizeof(DIR_ENTRY) * num_entries);
	init_insert_many_entries(0, num_entries, "test_file");
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	// Let num_entries of left node < MIN_DIR_ENTRIES_PER_PAGE
	num_entries = 0;
	pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[0]);
	tmp_node.num_entries = MIN_DIR_ENTRIES_PER_PAGE - 1;
	pwrite(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[0]);
	// Record entries in left node
	memcpy(remaining_entries, tmp_node.dir_entries, 
		sizeof(DIR_ENTRY) * tmp_node.num_entries);
	num_entries += tmp_node.num_entries;
	// Record entries in right node	
	pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[1]);
	memcpy(remaining_entries + num_entries, tmp_node.dir_entries, 
		sizeof(DIR_ENTRY) * tmp_node.num_entries);
	num_entries += tmp_node.num_entries;

	/* Run rebalancing function */
	ASSERT_EQ(1, rebalance_btree(&root_node, fh, &meta, 
		0, tmp_entries, tmp_child_pos));

	/* Check answer */
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	
	pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[0]);
	EXPECT_TRUE(tmp_node.num_entries > MIN_DIR_ENTRIES_PER_PAGE);
	pread(fh, &tmp_node, sizeof(DIR_ENTRY_PAGE), root_node.child_page_pos[1]);
	EXPECT_TRUE(tmp_node.num_entries > MIN_DIR_ENTRIES_PER_PAGE);
	for(int i = 0 ; i < num_entries ; i++) 
		ASSERT_EQ(true, search_entry(&remaining_entries[i]));
	EXPECT_EQ(0, meta.entry_page_gc_list);

	/* Free resource */
	free(remaining_entries);
}

/*
	End of unittest of rebalance_btree()
 */

/*
	Unittest of extract_largest_child()
 */

class extract_largest_childTest : public BaseClassInsertBtreeEntryIsUsable {
 
};

TEST_F(extract_largest_childTest, ExtractEmptyDirectory)
{
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY largest_child;
	
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);

	/* Run function */
	ASSERT_EQ(0, extract_largest_child(&root_node, fh, &meta, 
			&largest_child, tmp_entries, tmp_child_pos));
}

TEST_F(extract_largest_childTest, ExtractStartFromRootNode)
{	
	int num_entries = 20000;
	DIR_META_TYPE meta;
	DIR_ENTRY_PAGE root_node;
	DIR_ENTRY largest_child;

	init_insert_many_entries(0, num_entries, "test_file");
	pread(fh, &meta, sizeof(DIR_META_TYPE), sizeof(struct stat));
	pread(fh, &root_node, sizeof(DIR_ENTRY_PAGE), meta.root_entry_page);
	
	/* Run function */
	ASSERT_EQ(0, extract_largest_child(&root_node, fh, &meta, 
			&largest_child, tmp_entries, tmp_child_pos));

	/* Check answer */
	EXPECT_STREQ("test_file9999", largest_child.d_name) 
		<< "largest name = " << largest_child.d_name;
	EXPECT_EQ(false, search_entry(&largest_child));
}

/*
	End of unittest of extract_largest_child()
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>

extern "C" {
#include "dedup_table.h"
}

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



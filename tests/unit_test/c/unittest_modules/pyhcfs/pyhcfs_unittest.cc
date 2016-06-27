/* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved. */
#include <gtest/gtest.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
extern "C" {
#include "global.h"
#include "fuseop.h"
#include "time.h"
}

class pyhcfsTest : public ::testing::Test {
	protected:
	void SetUp() {
	}

	void TearDown() {
	}
};

TEST_F(pyhcfsTest, Traverse_Dir_Btree) {
 /* ASSERT_EQ(TRUE, hcfs_system->backend_is_online); */
}
TEST_F(pyhcfsTest, List_Dir_Inorder) {
 /* ASSERT_EQ(TRUE, hcfs_system->backend_is_online); */
}
TEST_F(pyhcfsTest, List_External_Volume) {
 /* ASSERT_EQ(TRUE, hcfs_system->backend_is_online); */
}
TEST_F(pyhcfsTest, Parse_Meta) {
 /* ASSERT_EQ(TRUE, hcfs_system->backend_is_online); */
}

/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: apk_mgmt_unittest.cc
* Abstract: 
* 	The unittest code for:
* 	1. Mimicing Full APK using Minimal APK
*
* Revision History
* 2016/12/07 Jethro Add unittest for apk_mgmt.c
*
**************************************************************************/
#include <gtest/gtest.h>

extern "C" {
#include "hfuse_system.h"
#include "fuseop.h"
#include "apk_mgmt.h"

#include "ut_helper.h"
}


/*
 * Helper functions
 */


/*
 * Google Tests
 */

void reset_fake_functions(void)
{
	reset_ut_helper();
}
void reset_unittest_env(void)
{
	hcfs_system->system_going_down = FALSE;
}

class UnittestEnvironment : public ::testing::Environment
{
	public:
	virtual void SetUp()
	{
		hcfs_system =
		    (SYSTEM_DATA_HEAD *)calloc(1, sizeof(SYSTEM_DATA_HEAD));
		reset_fake_functions();
	}

	virtual void TearDown() {}
};

::testing::Environment *const unitest_env =
    ::testing::AddGlobalTestEnvironment(new UnittestEnvironment);

class NotifyBufferSetUpAndTearDown : public ::testing::Test
{
	protected:
	virtual void SetUp() {
	}

	virtual void TearDown()
	{
		reset_unittest_env();
		reset_fake_functions();
	}
};

TEST(ToggleMinimalApkTest, On)
{
	hcfs_system->use_minimal_apk = 0;
	ASSERT_EQ(toggle_use_minimal_apk(1), 0);
	EXPECT_EQ(hcfs_system->use_minimal_apk, 1);
}
TEST(ToggleMinimalApkTest, Off)
{
	hcfs_system->use_minimal_apk = 1;
	EXPECT_EQ(toggle_use_minimal_apk(0), 0);
	EXPECT_EQ(hcfs_system->use_minimal_apk, 0);
}

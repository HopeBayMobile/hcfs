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

::testing::Environment *const fuseop_env =
    ::testing::AddGlobalTestEnvironment(new UnittestEnvironment);

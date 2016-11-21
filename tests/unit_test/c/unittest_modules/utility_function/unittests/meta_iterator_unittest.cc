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
}
#include "gtest/gtest.h"

class init_block_iterTest : public ::testing::Test {
protected:
	void SetUp()
	{
	}

	void TearDown()
	{
	}
};

TEST_F(init_block_iterTest, None)
{
}

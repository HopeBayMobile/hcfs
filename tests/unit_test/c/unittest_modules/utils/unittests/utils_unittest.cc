#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "utils.h"
#include "gtest/gtest.h"

// Tests non-existing file
TEST(check_file_size, Nonexist) {

  char temp[20];

  strcpy(temp,"nonexist");

  EXPECT_EQ(-1, check_file_size(temp));
}

/*
TEST(filelength, TestLength) {

  EXPECT_EQ(8,filelength("length8"));
}
*/

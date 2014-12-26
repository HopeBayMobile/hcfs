#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "utils.h"
}
#include "gtest/gtest.h"

// Tests non-existing file
TEST(check_file_size, Nonexist) {

  char temp[20];

  strcpy(temp,"nonexist");

  EXPECT_EQ(-1, check_file_size(temp));
}

TEST(check_file_size, Test_8_bytes) {

  EXPECT_EQ(8,check_file_size("testpatterns/size8bytes"));
}


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

// Unit test for Google Test test filters.
//
// A user can specify which test(s) in a Google Test program to run via
// either the GTEST_FILTER environment variable or the --gtest_filter
// flag.  This is used for testing such functionality.
//
// The program will be invoked from a Python unit test.  Don't run it
// directly.

#include "gtest/gtest.h"

namespace {

// Test case FooTest.

class FooTest : public testing::Test {
};

TEST_F(FooTest, Abc) {
}

TEST_F(FooTest, Xyz) {
  FAIL() << "Expected failure.";
}

// Test case BarTest.

TEST(BarTest, TestOne) {
}

TEST(BarTest, TestTwo) {
}

TEST(BarTest, TestThree) {
}

TEST(BarTest, DISABLED_TestFour) {
  FAIL() << "Expected failure.";
}

TEST(BarTest, DISABLED_TestFive) {
  FAIL() << "Expected failure.";
}

// Test case BazTest.

TEST(BazTest, TestOne) {
  FAIL() << "Expected failure.";
}

TEST(BazTest, TestA) {
}

TEST(BazTest, TestB) {
}

TEST(BazTest, DISABLED_TestC) {
  FAIL() << "Expected failure.";
}

// Test case HasDeathTest

TEST(HasDeathTest, Test1) {
  EXPECT_DEATH_IF_SUPPORTED(exit(1), ".*");
}

// We need at least two death tests to make sure that the all death tests
// aren't on the first shard.
TEST(HasDeathTest, Test2) {
  EXPECT_DEATH_IF_SUPPORTED(exit(1), ".*");
}

// Test case FoobarTest

TEST(DISABLED_FoobarTest, Test1) {
  FAIL() << "Expected failure.";
}

TEST(DISABLED_FoobarTest, DISABLED_Test2) {
  FAIL() << "Expected failure.";
}

// Test case FoobarbazTest

TEST(DISABLED_FoobarbazTest, TestA) {
  FAIL() << "Expected failure.";
}

#if GTEST_HAS_PARAM_TEST
class ParamTest : public testing::TestWithParam<int> {
};

TEST_P(ParamTest, TestX) {
}

TEST_P(ParamTest, TestY) {
}

INSTANTIATE_TEST_CASE_P(SeqP, ParamTest, testing::Values(1, 2));
INSTANTIATE_TEST_CASE_P(SeqQ, ParamTest, testing::Values(5, 6));
#endif  // GTEST_HAS_PARAM_TEST

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

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

// Tests the --gtest_repeat=number flag.

#include <stdlib.h>
#include <iostream>
#include "gtest/gtest.h"

// Indicates that this translation unit is part of Google Test's
// implementation.  It must come before gtest-internal-inl.h is
// included, or there will be a compiler error.  This trick is to
// prevent a user from accidentally including gtest-internal-inl.h in
// his code.
#define GTEST_IMPLEMENTATION_ 1
#include "src/gtest-internal-inl.h"
#undef GTEST_IMPLEMENTATION_

namespace testing {

GTEST_DECLARE_string_(death_test_style);
GTEST_DECLARE_string_(filter);
GTEST_DECLARE_int32_(repeat);

}  // namespace testing

using testing::GTEST_FLAG(death_test_style);
using testing::GTEST_FLAG(filter);
using testing::GTEST_FLAG(repeat);

namespace {

// We need this when we are testing Google Test itself and therefore
// cannot use Google Test assertions.
#define GTEST_CHECK_INT_EQ_(expected, actual) \
  do {\
    const int expected_val = (expected);\
    const int actual_val = (actual);\
    if (::testing::internal::IsTrue(expected_val != actual_val)) {\
      ::std::cout << "Value of: " #actual "\n"\
                  << "  Actual: " << actual_val << "\n"\
                  << "Expected: " #expected "\n"\
                  << "Which is: " << expected_val << "\n";\
      ::testing::internal::posix::Abort();\
    }\
  } while (::testing::internal::AlwaysFalse())


// Used for verifying that global environment set-up and tear-down are
// inside the gtest_repeat loop.

int g_environment_set_up_count = 0;
int g_environment_tear_down_count = 0;

class MyEnvironment : public testing::Environment {
 public:
  MyEnvironment() {}
  virtual void SetUp() { g_environment_set_up_count++; }
  virtual void TearDown() { g_environment_tear_down_count++; }
};

// A test that should fail.

int g_should_fail_count = 0;

TEST(FooTest, ShouldFail) {
  g_should_fail_count++;
  EXPECT_EQ(0, 1) << "Expected failure.";
}

// A test that should pass.

int g_should_pass_count = 0;

TEST(FooTest, ShouldPass) {
  g_should_pass_count++;
}

// A test that contains a thread-safe death test and a fast death
// test.  It should pass.

int g_death_test_count = 0;

TEST(BarDeathTest, ThreadSafeAndFast) {
  g_death_test_count++;

  GTEST_FLAG(death_test_style) = "threadsafe";
  EXPECT_DEATH_IF_SUPPORTED(::testing::internal::posix::Abort(), "");

  GTEST_FLAG(death_test_style) = "fast";
  EXPECT_DEATH_IF_SUPPORTED(::testing::internal::posix::Abort(), "");
}

#if GTEST_HAS_PARAM_TEST
int g_param_test_count = 0;

const int kNumberOfParamTests = 10;

class MyParamTest : public testing::TestWithParam<int> {};

TEST_P(MyParamTest, ShouldPass) {
  // TODO(vladl@google.com): Make parameter value checking robust
  //                         WRT order of tests.
  GTEST_CHECK_INT_EQ_(g_param_test_count % kNumberOfParamTests, GetParam());
  g_param_test_count++;
}
INSTANTIATE_TEST_CASE_P(MyParamSequence,
                        MyParamTest,
                        testing::Range(0, kNumberOfParamTests));
#endif  // GTEST_HAS_PARAM_TEST

// Resets the count for each test.
void ResetCounts() {
  g_environment_set_up_count = 0;
  g_environment_tear_down_count = 0;
  g_should_fail_count = 0;
  g_should_pass_count = 0;
  g_death_test_count = 0;
#if GTEST_HAS_PARAM_TEST
  g_param_test_count = 0;
#endif  // GTEST_HAS_PARAM_TEST
}

// Checks that the count for each test is expected.
void CheckCounts(int expected) {
  GTEST_CHECK_INT_EQ_(expected, g_environment_set_up_count);
  GTEST_CHECK_INT_EQ_(expected, g_environment_tear_down_count);
  GTEST_CHECK_INT_EQ_(expected, g_should_fail_count);
  GTEST_CHECK_INT_EQ_(expected, g_should_pass_count);
  GTEST_CHECK_INT_EQ_(expected, g_death_test_count);
#if GTEST_HAS_PARAM_TEST
  GTEST_CHECK_INT_EQ_(expected * kNumberOfParamTests, g_param_test_count);
#endif  // GTEST_HAS_PARAM_TEST
}

// Tests the behavior of Google Test when --gtest_repeat is not specified.
void TestRepeatUnspecified() {
  ResetCounts();
  GTEST_CHECK_INT_EQ_(1, RUN_ALL_TESTS());
  CheckCounts(1);
}

// Tests the behavior of Google Test when --gtest_repeat has the given value.
void TestRepeat(int repeat) {
  GTEST_FLAG(repeat) = repeat;

  ResetCounts();
  GTEST_CHECK_INT_EQ_(repeat > 0 ? 1 : 0, RUN_ALL_TESTS());
  CheckCounts(repeat);
}

// Tests using --gtest_repeat when --gtest_filter specifies an empty
// set of tests.
void TestRepeatWithEmptyFilter(int repeat) {
  GTEST_FLAG(repeat) = repeat;
  GTEST_FLAG(filter) = "None";

  ResetCounts();
  GTEST_CHECK_INT_EQ_(0, RUN_ALL_TESTS());
  CheckCounts(0);
}

// Tests using --gtest_repeat when --gtest_filter specifies a set of
// successful tests.
void TestRepeatWithFilterForSuccessfulTests(int repeat) {
  GTEST_FLAG(repeat) = repeat;
  GTEST_FLAG(filter) = "*-*ShouldFail";

  ResetCounts();
  GTEST_CHECK_INT_EQ_(0, RUN_ALL_TESTS());
  GTEST_CHECK_INT_EQ_(repeat, g_environment_set_up_count);
  GTEST_CHECK_INT_EQ_(repeat, g_environment_tear_down_count);
  GTEST_CHECK_INT_EQ_(0, g_should_fail_count);
  GTEST_CHECK_INT_EQ_(repeat, g_should_pass_count);
  GTEST_CHECK_INT_EQ_(repeat, g_death_test_count);
#if GTEST_HAS_PARAM_TEST
  GTEST_CHECK_INT_EQ_(repeat * kNumberOfParamTests, g_param_test_count);
#endif  // GTEST_HAS_PARAM_TEST
}

// Tests using --gtest_repeat when --gtest_filter specifies a set of
// failed tests.
void TestRepeatWithFilterForFailedTests(int repeat) {
  GTEST_FLAG(repeat) = repeat;
  GTEST_FLAG(filter) = "*ShouldFail";

  ResetCounts();
  GTEST_CHECK_INT_EQ_(1, RUN_ALL_TESTS());
  GTEST_CHECK_INT_EQ_(repeat, g_environment_set_up_count);
  GTEST_CHECK_INT_EQ_(repeat, g_environment_tear_down_count);
  GTEST_CHECK_INT_EQ_(repeat, g_should_fail_count);
  GTEST_CHECK_INT_EQ_(0, g_should_pass_count);
  GTEST_CHECK_INT_EQ_(0, g_death_test_count);
#if GTEST_HAS_PARAM_TEST
  GTEST_CHECK_INT_EQ_(0, g_param_test_count);
#endif  // GTEST_HAS_PARAM_TEST
}

}  // namespace

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::AddGlobalTestEnvironment(new MyEnvironment);

  TestRepeatUnspecified();
  TestRepeat(0);
  TestRepeat(1);
  TestRepeat(5);

  TestRepeatWithEmptyFilter(2);
  TestRepeatWithEmptyFilter(3);

  TestRepeatWithFilterForSuccessfulTests(3);

  TestRepeatWithFilterForFailedTests(4);

  // It would be nice to verify that the tests indeed loop forever
  // when GTEST_FLAG(repeat) is negative, but this test will be quite
  // complicated to write.  Since this flag is for interactive
  // debugging only and doesn't affect the normal test result, such a
  // test would be an overkill.

  printf("PASS\n");
  return 0;
}

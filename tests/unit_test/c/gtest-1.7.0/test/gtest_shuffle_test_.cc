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

// Verifies that test shuffling works.

#include "gtest/gtest.h"

namespace {

using ::testing::EmptyTestEventListener;
using ::testing::InitGoogleTest;
using ::testing::Message;
using ::testing::Test;
using ::testing::TestEventListeners;
using ::testing::TestInfo;
using ::testing::UnitTest;
using ::testing::internal::scoped_ptr;

// The test methods are empty, as the sole purpose of this program is
// to print the test names before/after shuffling.

class A : public Test {};
TEST_F(A, A) {}
TEST_F(A, B) {}

TEST(ADeathTest, A) {}
TEST(ADeathTest, B) {}
TEST(ADeathTest, C) {}

TEST(B, A) {}
TEST(B, B) {}
TEST(B, C) {}
TEST(B, DISABLED_D) {}
TEST(B, DISABLED_E) {}

TEST(BDeathTest, A) {}
TEST(BDeathTest, B) {}

TEST(C, A) {}
TEST(C, B) {}
TEST(C, C) {}
TEST(C, DISABLED_D) {}

TEST(CDeathTest, A) {}

TEST(DISABLED_D, A) {}
TEST(DISABLED_D, DISABLED_B) {}

// This printer prints the full test names only, starting each test
// iteration with a "----" marker.
class TestNamePrinter : public EmptyTestEventListener {
 public:
  virtual void OnTestIterationStart(const UnitTest& /* unit_test */,
                                    int /* iteration */) {
    printf("----\n");
  }

  virtual void OnTestStart(const TestInfo& test_info) {
    printf("%s.%s\n", test_info.test_case_name(), test_info.name());
  }
};

}  // namespace

int main(int argc, char **argv) {
  InitGoogleTest(&argc, argv);

  // Replaces the default printer with TestNamePrinter, which prints
  // the test name only.
  TestEventListeners& listeners = UnitTest::GetInstance()->listeners();
  delete listeners.Release(listeners.default_result_printer());
  listeners.Append(new TestNamePrinter);

  return RUN_ALL_TESTS();
}

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

// A helper program for testing that Google Test parses the environment
// variables correctly.

#include "gtest/gtest.h"

#include <iostream>

#define GTEST_IMPLEMENTATION_ 1
#include "src/gtest-internal-inl.h"
#undef GTEST_IMPLEMENTATION_

using ::std::cout;

namespace testing {

// The purpose of this is to make the test more realistic by ensuring
// that the UnitTest singleton is created before main() is entered.
// We don't actual run the TEST itself.
TEST(GTestEnvVarTest, Dummy) {
}

void PrintFlag(const char* flag) {
  if (strcmp(flag, "break_on_failure") == 0) {
    cout << GTEST_FLAG(break_on_failure);
    return;
  }

  if (strcmp(flag, "catch_exceptions") == 0) {
    cout << GTEST_FLAG(catch_exceptions);
    return;
  }

  if (strcmp(flag, "color") == 0) {
    cout << GTEST_FLAG(color);
    return;
  }

  if (strcmp(flag, "death_test_style") == 0) {
    cout << GTEST_FLAG(death_test_style);
    return;
  }

  if (strcmp(flag, "death_test_use_fork") == 0) {
    cout << GTEST_FLAG(death_test_use_fork);
    return;
  }

  if (strcmp(flag, "filter") == 0) {
    cout << GTEST_FLAG(filter);
    return;
  }

  if (strcmp(flag, "output") == 0) {
    cout << GTEST_FLAG(output);
    return;
  }

  if (strcmp(flag, "print_time") == 0) {
    cout << GTEST_FLAG(print_time);
    return;
  }

  if (strcmp(flag, "repeat") == 0) {
    cout << GTEST_FLAG(repeat);
    return;
  }

  if (strcmp(flag, "stack_trace_depth") == 0) {
    cout << GTEST_FLAG(stack_trace_depth);
    return;
  }

  if (strcmp(flag, "throw_on_failure") == 0) {
    cout << GTEST_FLAG(throw_on_failure);
    return;
  }

  cout << "Invalid flag name " << flag
       << ".  Valid names are break_on_failure, color, filter, etc.\n";
  exit(1);
}

}  // namespace testing

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  if (argc != 2) {
    cout << "Usage: gtest_env_var_test_ NAME_OF_FLAG\n";
    return 1;
  }

  testing::PrintFlag(argv[1]);
  return 0;
}

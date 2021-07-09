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

// Tests that a Google Test program that has no test defined can run
// successfully.
//
// Author: wan@google.com (Zhanyong Wan)

#include "gtest/gtest.h"

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  // An ad-hoc assertion outside of all tests.
  //
  // This serves three purposes:
  //
  // 1. It verifies that an ad-hoc assertion can be executed even if
  //    no test is defined.
  // 2. It verifies that a failed ad-hoc assertion causes the test
  //    program to fail.
  // 3. We had a bug where the XML output won't be generated if an
  //    assertion is executed before RUN_ALL_TESTS() is called, even
  //    though --gtest_output=xml is specified.  This makes sure the
  //    bug is fixed and doesn't regress.
  EXPECT_EQ(1, 2);

  // The above EXPECT_EQ() should cause RUN_ALL_TESTS() to return non-zero.
  return RUN_ALL_TESTS() ? 0 : 1;
}

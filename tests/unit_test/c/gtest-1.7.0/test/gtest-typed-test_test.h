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

#ifndef GTEST_TEST_GTEST_TYPED_TEST_TEST_H_
#define GTEST_TEST_GTEST_TYPED_TEST_TEST_H_

#include "gtest/gtest.h"

#if GTEST_HAS_TYPED_TEST_P

using testing::Test;

// For testing that the same type-parameterized test case can be
// instantiated in different translation units linked together.
// ContainerTest will be instantiated in both gtest-typed-test_test.cc
// and gtest-typed-test2_test.cc.

template <typename T>
class ContainerTest : public Test {
};

TYPED_TEST_CASE_P(ContainerTest);

TYPED_TEST_P(ContainerTest, CanBeDefaultConstructed) {
  TypeParam container;
}

TYPED_TEST_P(ContainerTest, InitialSizeIsZero) {
  TypeParam container;
  EXPECT_EQ(0U, container.size());
}

REGISTER_TYPED_TEST_CASE_P(ContainerTest,
                           CanBeDefaultConstructed, InitialSizeIsZero);

#endif  // GTEST_HAS_TYPED_TEST_P

#endif  // GTEST_TEST_GTEST_TYPED_TEST_TEST_H_

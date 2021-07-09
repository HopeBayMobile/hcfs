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

#include <vector>

#include "test/gtest-typed-test_test.h"
#include "gtest/gtest.h"

#if GTEST_HAS_TYPED_TEST_P

// Tests that the same type-parameterized test case can be
// instantiated in different translation units linked together.
// (ContainerTest is also instantiated in gtest-typed-test_test.cc.)
INSTANTIATE_TYPED_TEST_CASE_P(Vector, ContainerTest,
                              testing::Types<std::vector<int> >);

#endif  // GTEST_HAS_TYPED_TEST_P

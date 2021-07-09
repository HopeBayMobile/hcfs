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

#ifndef GTEST_TEST_GTEST_PARAM_TEST_TEST_H_
#define GTEST_TEST_GTEST_PARAM_TEST_TEST_H_

#include "gtest/gtest.h"

#if GTEST_HAS_PARAM_TEST

// Test fixture for testing definition and instantiation of a test
// in separate translation units.
class ExternalInstantiationTest : public ::testing::TestWithParam<int> {
};

// Test fixture for testing instantiation of a test in multiple
// translation units.
class InstantiationInMultipleTranslaionUnitsTest
    : public ::testing::TestWithParam<int> {
};

#endif  // GTEST_HAS_PARAM_TEST

#endif  // GTEST_TEST_GTEST_PARAM_TEST_TEST_H_

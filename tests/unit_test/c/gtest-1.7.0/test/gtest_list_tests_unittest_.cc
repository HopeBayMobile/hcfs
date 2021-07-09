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

// Unit test for Google Test's --gtest_list_tests flag.
//
// A user can ask Google Test to list all tests that will run
// so that when using a filter, a user will know what
// tests to look for. The tests will not be run after listing.
//
// This program will be invoked from a Python unit test.
// Don't run it directly.

#include "gtest/gtest.h"

// Several different test cases and tests that will be listed.
TEST(Foo, Bar1) {
}

TEST(Foo, Bar2) {
}

TEST(Foo, DISABLED_Bar3) {
}

TEST(Abc, Xyz) {
}

TEST(Abc, Def) {
}

TEST(FooBar, Baz) {
}

class FooTest : public testing::Test {
};

TEST_F(FooTest, Test1) {
}

TEST_F(FooTest, DISABLED_Test2) {
}

TEST_F(FooTest, Test3) {
}

TEST(FooDeathTest, Test1) {
}

// A group of value-parameterized tests.

class MyType {
 public:
  explicit MyType(const std::string& a_value) : value_(a_value) {}

  const std::string& value() const { return value_; }

 private:
  std::string value_;
};

// Teaches Google Test how to print a MyType.
void PrintTo(const MyType& x, std::ostream* os) {
  *os << x.value();
}

class ValueParamTest : public testing::TestWithParam<MyType> {
};

TEST_P(ValueParamTest, TestA) {
}

TEST_P(ValueParamTest, TestB) {
}

INSTANTIATE_TEST_CASE_P(
    MyInstantiation, ValueParamTest,
    testing::Values(MyType("one line"),
                    MyType("two\nlines"),
                    MyType("a very\nloooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooong line")));  // NOLINT

// A group of typed tests.

// A deliberately long type name for testing the line-truncating
// behavior when printing a type parameter.
class VeryLoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooogName {  // NOLINT
};

template <typename T>
class TypedTest : public testing::Test {
};

template <typename T, int kSize>
class MyArray {
};

typedef testing::Types<VeryLoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooogName,  // NOLINT
                       int*, MyArray<bool, 42> > MyTypes;

TYPED_TEST_CASE(TypedTest, MyTypes);

TYPED_TEST(TypedTest, TestA) {
}

TYPED_TEST(TypedTest, TestB) {
}

// A group of type-parameterized tests.

template <typename T>
class TypeParamTest : public testing::Test {
};

TYPED_TEST_CASE_P(TypeParamTest);

TYPED_TEST_P(TypeParamTest, TestA) {
}

TYPED_TEST_P(TypeParamTest, TestB) {
}

REGISTER_TYPED_TEST_CASE_P(TypeParamTest, TestA, TestB);

INSTANTIATE_TYPED_TEST_CASE_P(My, TypeParamTest, MyTypes);

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

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

#include <set>
#include <vector>

#include "test/gtest-typed-test_test.h"
#include "gtest/gtest.h"

using testing::Test;

// Used for testing that SetUpTestCase()/TearDownTestCase(), fixture
// ctor/dtor, and SetUp()/TearDown() work correctly in typed tests and
// type-parameterized test.
template <typename T>
class CommonTest : public Test {
  // For some technical reason, SetUpTestCase() and TearDownTestCase()
  // must be public.
 public:
  static void SetUpTestCase() {
    shared_ = new T(5);
  }

  static void TearDownTestCase() {
    delete shared_;
    shared_ = NULL;
  }

  // This 'protected:' is optional.  There's no harm in making all
  // members of this fixture class template public.
 protected:
  // We used to use std::list here, but switched to std::vector since
  // MSVC's <list> doesn't compile cleanly with /W4.
  typedef std::vector<T> Vector;
  typedef std::set<int> IntSet;

  CommonTest() : value_(1) {}

  virtual ~CommonTest() { EXPECT_EQ(3, value_); }

  virtual void SetUp() {
    EXPECT_EQ(1, value_);
    value_++;
  }

  virtual void TearDown() {
    EXPECT_EQ(2, value_);
    value_++;
  }

  T value_;
  static T* shared_;
};

template <typename T>
T* CommonTest<T>::shared_ = NULL;

// This #ifdef block tests typed tests.
#if GTEST_HAS_TYPED_TEST

using testing::Types;

// Tests that SetUpTestCase()/TearDownTestCase(), fixture ctor/dtor,
// and SetUp()/TearDown() work correctly in typed tests

typedef Types<char, int> TwoTypes;
TYPED_TEST_CASE(CommonTest, TwoTypes);

TYPED_TEST(CommonTest, ValuesAreCorrect) {
  // Static members of the fixture class template can be visited via
  // the TestFixture:: prefix.
  EXPECT_EQ(5, *TestFixture::shared_);

  // Typedefs in the fixture class template can be visited via the
  // "typename TestFixture::" prefix.
  typename TestFixture::Vector empty;
  EXPECT_EQ(0U, empty.size());

  typename TestFixture::IntSet empty2;
  EXPECT_EQ(0U, empty2.size());

  // Non-static members of the fixture class must be visited via
  // 'this', as required by C++ for class templates.
  EXPECT_EQ(2, this->value_);
}

// The second test makes sure shared_ is not deleted after the first
// test.
TYPED_TEST(CommonTest, ValuesAreStillCorrect) {
  // Static members of the fixture class template can also be visited
  // via 'this'.
  ASSERT_TRUE(this->shared_ != NULL);
  EXPECT_EQ(5, *this->shared_);

  // TypeParam can be used to refer to the type parameter.
  EXPECT_EQ(static_cast<TypeParam>(2), this->value_);
}

// Tests that multiple TYPED_TEST_CASE's can be defined in the same
// translation unit.

template <typename T>
class TypedTest1 : public Test {
};

// Verifies that the second argument of TYPED_TEST_CASE can be a
// single type.
TYPED_TEST_CASE(TypedTest1, int);
TYPED_TEST(TypedTest1, A) {}

template <typename T>
class TypedTest2 : public Test {
};

// Verifies that the second argument of TYPED_TEST_CASE can be a
// Types<...> type list.
TYPED_TEST_CASE(TypedTest2, Types<int>);

// This also verifies that tests from different typed test cases can
// share the same name.
TYPED_TEST(TypedTest2, A) {}

// Tests that a typed test case can be defined in a namespace.

namespace library1 {

template <typename T>
class NumericTest : public Test {
};

typedef Types<int, long> NumericTypes;
TYPED_TEST_CASE(NumericTest, NumericTypes);

TYPED_TEST(NumericTest, DefaultIsZero) {
  EXPECT_EQ(0, TypeParam());
}

}  // namespace library1

#endif  // GTEST_HAS_TYPED_TEST

// This #ifdef block tests type-parameterized tests.
#if GTEST_HAS_TYPED_TEST_P

using testing::Types;
using testing::internal::TypedTestCasePState;

// Tests TypedTestCasePState.

class TypedTestCasePStateTest : public Test {
 protected:
  virtual void SetUp() {
    state_.AddTestName("foo.cc", 0, "FooTest", "A");
    state_.AddTestName("foo.cc", 0, "FooTest", "B");
    state_.AddTestName("foo.cc", 0, "FooTest", "C");
  }

  TypedTestCasePState state_;
};

TEST_F(TypedTestCasePStateTest, SucceedsForMatchingList) {
  const char* tests = "A, B, C";
  EXPECT_EQ(tests,
            state_.VerifyRegisteredTestNames("foo.cc", 1, tests));
}

// Makes sure that the order of the tests and spaces around the names
// don't matter.
TEST_F(TypedTestCasePStateTest, IgnoresOrderAndSpaces) {
  const char* tests = "A,C,   B";
  EXPECT_EQ(tests,
            state_.VerifyRegisteredTestNames("foo.cc", 1, tests));
}

typedef TypedTestCasePStateTest TypedTestCasePStateDeathTest;

TEST_F(TypedTestCasePStateDeathTest, DetectsDuplicates) {
  EXPECT_DEATH_IF_SUPPORTED(
      state_.VerifyRegisteredTestNames("foo.cc", 1, "A, B, A, C"),
      "foo\\.cc.1.?: Test A is listed more than once\\.");
}

TEST_F(TypedTestCasePStateDeathTest, DetectsExtraTest) {
  EXPECT_DEATH_IF_SUPPORTED(
      state_.VerifyRegisteredTestNames("foo.cc", 1, "A, B, C, D"),
      "foo\\.cc.1.?: No test named D can be found in this test case\\.");
}

TEST_F(TypedTestCasePStateDeathTest, DetectsMissedTest) {
  EXPECT_DEATH_IF_SUPPORTED(
      state_.VerifyRegisteredTestNames("foo.cc", 1, "A, C"),
      "foo\\.cc.1.?: You forgot to list test B\\.");
}

// Tests that defining a test for a parameterized test case generates
// a run-time error if the test case has been registered.
TEST_F(TypedTestCasePStateDeathTest, DetectsTestAfterRegistration) {
  state_.VerifyRegisteredTestNames("foo.cc", 1, "A, B, C");
  EXPECT_DEATH_IF_SUPPORTED(
      state_.AddTestName("foo.cc", 2, "FooTest", "D"),
      "foo\\.cc.2.?: Test D must be defined before REGISTER_TYPED_TEST_CASE_P"
      "\\(FooTest, \\.\\.\\.\\)\\.");
}

// Tests that SetUpTestCase()/TearDownTestCase(), fixture ctor/dtor,
// and SetUp()/TearDown() work correctly in type-parameterized tests.

template <typename T>
class DerivedTest : public CommonTest<T> {
};

TYPED_TEST_CASE_P(DerivedTest);

TYPED_TEST_P(DerivedTest, ValuesAreCorrect) {
  // Static members of the fixture class template can be visited via
  // the TestFixture:: prefix.
  EXPECT_EQ(5, *TestFixture::shared_);

  // Non-static members of the fixture class must be visited via
  // 'this', as required by C++ for class templates.
  EXPECT_EQ(2, this->value_);
}

// The second test makes sure shared_ is not deleted after the first
// test.
TYPED_TEST_P(DerivedTest, ValuesAreStillCorrect) {
  // Static members of the fixture class template can also be visited
  // via 'this'.
  ASSERT_TRUE(this->shared_ != NULL);
  EXPECT_EQ(5, *this->shared_);
  EXPECT_EQ(2, this->value_);
}

REGISTER_TYPED_TEST_CASE_P(DerivedTest,
                           ValuesAreCorrect, ValuesAreStillCorrect);

typedef Types<short, long> MyTwoTypes;
INSTANTIATE_TYPED_TEST_CASE_P(My, DerivedTest, MyTwoTypes);

// Tests that multiple TYPED_TEST_CASE_P's can be defined in the same
// translation unit.

template <typename T>
class TypedTestP1 : public Test {
};

TYPED_TEST_CASE_P(TypedTestP1);

// For testing that the code between TYPED_TEST_CASE_P() and
// TYPED_TEST_P() is not enclosed in a namespace.
typedef int IntAfterTypedTestCaseP;

TYPED_TEST_P(TypedTestP1, A) {}
TYPED_TEST_P(TypedTestP1, B) {}

// For testing that the code between TYPED_TEST_P() and
// REGISTER_TYPED_TEST_CASE_P() is not enclosed in a namespace.
typedef int IntBeforeRegisterTypedTestCaseP;

REGISTER_TYPED_TEST_CASE_P(TypedTestP1, A, B);

template <typename T>
class TypedTestP2 : public Test {
};

TYPED_TEST_CASE_P(TypedTestP2);

// This also verifies that tests from different type-parameterized
// test cases can share the same name.
TYPED_TEST_P(TypedTestP2, A) {}

REGISTER_TYPED_TEST_CASE_P(TypedTestP2, A);

// Verifies that the code between TYPED_TEST_CASE_P() and
// REGISTER_TYPED_TEST_CASE_P() is not enclosed in a namespace.
IntAfterTypedTestCaseP after = 0;
IntBeforeRegisterTypedTestCaseP before = 0;

// Verifies that the last argument of INSTANTIATE_TYPED_TEST_CASE_P()
// can be either a single type or a Types<...> type list.
INSTANTIATE_TYPED_TEST_CASE_P(Int, TypedTestP1, int);
INSTANTIATE_TYPED_TEST_CASE_P(Int, TypedTestP2, Types<int>);

// Tests that the same type-parameterized test case can be
// instantiated more than once in the same translation unit.
INSTANTIATE_TYPED_TEST_CASE_P(Double, TypedTestP2, Types<double>);

// Tests that the same type-parameterized test case can be
// instantiated in different translation units linked together.
// (ContainerTest is also instantiated in gtest-typed-test_test.cc.)
typedef Types<std::vector<double>, std::set<char> > MyContainers;
INSTANTIATE_TYPED_TEST_CASE_P(My, ContainerTest, MyContainers);

// Tests that a type-parameterized test case can be defined and
// instantiated in a namespace.

namespace library2 {

template <typename T>
class NumericTest : public Test {
};

TYPED_TEST_CASE_P(NumericTest);

TYPED_TEST_P(NumericTest, DefaultIsZero) {
  EXPECT_EQ(0, TypeParam());
}

TYPED_TEST_P(NumericTest, ZeroIsLessThanOne) {
  EXPECT_LT(TypeParam(0), TypeParam(1));
}

REGISTER_TYPED_TEST_CASE_P(NumericTest,
                           DefaultIsZero, ZeroIsLessThanOne);
typedef Types<int, double> NumericTypes;
INSTANTIATE_TYPED_TEST_CASE_P(My, NumericTest, NumericTypes);

}  // namespace library2

#endif  // GTEST_HAS_TYPED_TEST_P

#if !defined(GTEST_HAS_TYPED_TEST) && !defined(GTEST_HAS_TYPED_TEST_P)

// Google Test may not support type-parameterized tests with some
// compilers. If we use conditional compilation to compile out all
// code referring to the gtest_main library, MSVC linker will not link
// that library at all and consequently complain about missing entry
// point defined in that library (fatal error LNK1561: entry point
// must be defined). This dummy test keeps gtest_main linked in.
TEST(DummyTest, TypedTestsAreNotSupportedOnThisPlatform) {}

#endif  // #if !defined(GTEST_HAS_TYPED_TEST) && !defined(GTEST_HAS_TYPED_TEST_P)

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

#include "gtest/gtest-test-part.h"

#include "gtest/gtest.h"

using testing::Message;
using testing::Test;
using testing::TestPartResult;
using testing::TestPartResultArray;

namespace {

// Tests the TestPartResult class.

// The test fixture for testing TestPartResult.
class TestPartResultTest : public Test {
 protected:
  TestPartResultTest()
      : r1_(TestPartResult::kSuccess, "foo/bar.cc", 10, "Success!"),
        r2_(TestPartResult::kNonFatalFailure, "foo/bar.cc", -1, "Failure!"),
        r3_(TestPartResult::kFatalFailure, NULL, -1, "Failure!") {}

  TestPartResult r1_, r2_, r3_;
};


TEST_F(TestPartResultTest, ConstructorWorks) {
  Message message;
  message << "something is terribly wrong";
  message << static_cast<const char*>(testing::internal::kStackTraceMarker);
  message << "some unimportant stack trace";

  const TestPartResult result(TestPartResult::kNonFatalFailure,
                              "some_file.cc",
                              42,
                              message.GetString().c_str());

  EXPECT_EQ(TestPartResult::kNonFatalFailure, result.type());
  EXPECT_STREQ("some_file.cc", result.file_name());
  EXPECT_EQ(42, result.line_number());
  EXPECT_STREQ(message.GetString().c_str(), result.message());
  EXPECT_STREQ("something is terribly wrong", result.summary());
}

TEST_F(TestPartResultTest, ResultAccessorsWork) {
  const TestPartResult success(TestPartResult::kSuccess,
                               "file.cc",
                               42,
                               "message");
  EXPECT_TRUE(success.passed());
  EXPECT_FALSE(success.failed());
  EXPECT_FALSE(success.nonfatally_failed());
  EXPECT_FALSE(success.fatally_failed());

  const TestPartResult nonfatal_failure(TestPartResult::kNonFatalFailure,
                                        "file.cc",
                                        42,
                                        "message");
  EXPECT_FALSE(nonfatal_failure.passed());
  EXPECT_TRUE(nonfatal_failure.failed());
  EXPECT_TRUE(nonfatal_failure.nonfatally_failed());
  EXPECT_FALSE(nonfatal_failure.fatally_failed());

  const TestPartResult fatal_failure(TestPartResult::kFatalFailure,
                                     "file.cc",
                                     42,
                                     "message");
  EXPECT_FALSE(fatal_failure.passed());
  EXPECT_TRUE(fatal_failure.failed());
  EXPECT_FALSE(fatal_failure.nonfatally_failed());
  EXPECT_TRUE(fatal_failure.fatally_failed());
}

// Tests TestPartResult::type().
TEST_F(TestPartResultTest, type) {
  EXPECT_EQ(TestPartResult::kSuccess, r1_.type());
  EXPECT_EQ(TestPartResult::kNonFatalFailure, r2_.type());
  EXPECT_EQ(TestPartResult::kFatalFailure, r3_.type());
}

// Tests TestPartResult::file_name().
TEST_F(TestPartResultTest, file_name) {
  EXPECT_STREQ("foo/bar.cc", r1_.file_name());
  EXPECT_STREQ(NULL, r3_.file_name());
}

// Tests TestPartResult::line_number().
TEST_F(TestPartResultTest, line_number) {
  EXPECT_EQ(10, r1_.line_number());
  EXPECT_EQ(-1, r2_.line_number());
}

// Tests TestPartResult::message().
TEST_F(TestPartResultTest, message) {
  EXPECT_STREQ("Success!", r1_.message());
}

// Tests TestPartResult::passed().
TEST_F(TestPartResultTest, Passed) {
  EXPECT_TRUE(r1_.passed());
  EXPECT_FALSE(r2_.passed());
  EXPECT_FALSE(r3_.passed());
}

// Tests TestPartResult::failed().
TEST_F(TestPartResultTest, Failed) {
  EXPECT_FALSE(r1_.failed());
  EXPECT_TRUE(r2_.failed());
  EXPECT_TRUE(r3_.failed());
}

// Tests TestPartResult::fatally_failed().
TEST_F(TestPartResultTest, FatallyFailed) {
  EXPECT_FALSE(r1_.fatally_failed());
  EXPECT_FALSE(r2_.fatally_failed());
  EXPECT_TRUE(r3_.fatally_failed());
}

// Tests TestPartResult::nonfatally_failed().
TEST_F(TestPartResultTest, NonfatallyFailed) {
  EXPECT_FALSE(r1_.nonfatally_failed());
  EXPECT_TRUE(r2_.nonfatally_failed());
  EXPECT_FALSE(r3_.nonfatally_failed());
}

// Tests the TestPartResultArray class.

class TestPartResultArrayTest : public Test {
 protected:
  TestPartResultArrayTest()
      : r1_(TestPartResult::kNonFatalFailure, "foo/bar.cc", -1, "Failure 1"),
        r2_(TestPartResult::kFatalFailure, "foo/bar.cc", -1, "Failure 2") {}

  const TestPartResult r1_, r2_;
};

// Tests that TestPartResultArray initially has size 0.
TEST_F(TestPartResultArrayTest, InitialSizeIsZero) {
  TestPartResultArray results;
  EXPECT_EQ(0, results.size());
}

// Tests that TestPartResultArray contains the given TestPartResult
// after one Append() operation.
TEST_F(TestPartResultArrayTest, ContainsGivenResultAfterAppend) {
  TestPartResultArray results;
  results.Append(r1_);
  EXPECT_EQ(1, results.size());
  EXPECT_STREQ("Failure 1", results.GetTestPartResult(0).message());
}

// Tests that TestPartResultArray contains the given TestPartResults
// after two Append() operations.
TEST_F(TestPartResultArrayTest, ContainsGivenResultsAfterTwoAppends) {
  TestPartResultArray results;
  results.Append(r1_);
  results.Append(r2_);
  EXPECT_EQ(2, results.size());
  EXPECT_STREQ("Failure 1", results.GetTestPartResult(0).message());
  EXPECT_STREQ("Failure 2", results.GetTestPartResult(1).message());
}

typedef TestPartResultArrayTest TestPartResultArrayDeathTest;

// Tests that the program dies when GetTestPartResult() is called with
// an invalid index.
TEST_F(TestPartResultArrayDeathTest, DiesWhenIndexIsOutOfBound) {
  TestPartResultArray results;
  results.Append(r1_);

  EXPECT_DEATH_IF_SUPPORTED(results.GetTestPartResult(-1), "");
  EXPECT_DEATH_IF_SUPPORTED(results.GetTestPartResult(1), "");
}

// TODO(mheule@google.com): Add a test for the class HasNewFatalFailureHelper.

}  // namespace

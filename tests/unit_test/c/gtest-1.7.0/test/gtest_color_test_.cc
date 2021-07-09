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

// A helper program for testing how Google Test determines whether to use
// colors in the output.  It prints "YES" and returns 1 if Google Test
// decides to use colors, and prints "NO" and returns 0 otherwise.

#include <stdio.h>

#include "gtest/gtest.h"

// Indicates that this translation unit is part of Google Test's
// implementation.  It must come before gtest-internal-inl.h is
// included, or there will be a compiler error.  This trick is to
// prevent a user from accidentally including gtest-internal-inl.h in
// his code.
#define GTEST_IMPLEMENTATION_ 1
#include "src/gtest-internal-inl.h"
#undef GTEST_IMPLEMENTATION_

using testing::internal::ShouldUseColor;

// The purpose of this is to ensure that the UnitTest singleton is
// created before main() is entered, and thus that ShouldUseColor()
// works the same way as in a real Google-Test-based test.  We don't actual
// run the TEST itself.
TEST(GTestColorTest, Dummy) {
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  if (ShouldUseColor(true)) {
    // Google Test decides to use colors in the output (assuming it
    // goes to a TTY).
    printf("YES\n");
    return 1;
  } else {
    // Google Test decides not to use colors in the output.
    printf("NO\n");
    return 0;
  }
}

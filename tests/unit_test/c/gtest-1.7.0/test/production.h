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

#ifndef GTEST_TEST_PRODUCTION_H_
#define GTEST_TEST_PRODUCTION_H_

#include "gtest/gtest_prod.h"

class PrivateCode {
 public:
  // Declares a friend test that does not use a fixture.
  FRIEND_TEST(PrivateCodeTest, CanAccessPrivateMembers);

  // Declares a friend test that uses a fixture.
  FRIEND_TEST(PrivateCodeFixtureTest, CanAccessPrivateMembers);

  PrivateCode();

  int x() const { return x_; }
 private:
  void set_x(int an_x) { x_ = an_x; }
  int x_;
};

#endif  // GTEST_TEST_PRODUCTION_H_

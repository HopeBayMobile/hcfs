#!/usr/bin/env python
#
# Copyright (c) 2021 HopeBayTech.
#
# This file is part of Tera.
# See https://github.com/HopeBayMobile for further info.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Verifies that Google Test warns the user when not initialized properly."""

__author__ = 'wan@google.com (Zhanyong Wan)'

import gtest_test_utils


COMMAND = gtest_test_utils.GetTestExecutablePath('gtest_uninitialized_test_')


def Assert(condition):
  if not condition:
    raise AssertionError


def AssertEq(expected, actual):
  if expected != actual:
    print 'Expected: %s' % (expected,)
    print '  Actual: %s' % (actual,)
    raise AssertionError


def TestExitCodeAndOutput(command):
  """Runs the given command and verifies its exit code and output."""

  # Verifies that 'command' exits with code 1.
  p = gtest_test_utils.Subprocess(command)
  Assert(p.exited)
  AssertEq(1, p.exit_code)
  Assert('InitGoogleTest' in p.output)


class GTestUninitializedTest(gtest_test_utils.TestCase):
  def testExitCodeAndOutput(self):
    TestExitCodeAndOutput(COMMAND)


if __name__ == '__main__':
  gtest_test_utils.Main()

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

"""Verifies that Google Test correctly parses environment variables."""

__author__ = 'wan@google.com (Zhanyong Wan)'

import os
import gtest_test_utils


IS_WINDOWS = os.name == 'nt'
IS_LINUX = os.name == 'posix' and os.uname()[0] == 'Linux'

COMMAND = gtest_test_utils.GetTestExecutablePath('gtest_env_var_test_')

environ = os.environ.copy()


def AssertEq(expected, actual):
  if expected != actual:
    print 'Expected: %s' % (expected,)
    print '  Actual: %s' % (actual,)
    raise AssertionError


def SetEnvVar(env_var, value):
  """Sets the env variable to 'value'; unsets it when 'value' is None."""

  if value is not None:
    environ[env_var] = value
  elif env_var in environ:
    del environ[env_var]


def GetFlag(flag):
  """Runs gtest_env_var_test_ and returns its output."""

  args = [COMMAND]
  if flag is not None:
    args += [flag]
  return gtest_test_utils.Subprocess(args, env=environ).output


def TestFlag(flag, test_val, default_val):
  """Verifies that the given flag is affected by the corresponding env var."""

  env_var = 'GTEST_' + flag.upper()
  SetEnvVar(env_var, test_val)
  AssertEq(test_val, GetFlag(flag))
  SetEnvVar(env_var, None)
  AssertEq(default_val, GetFlag(flag))


class GTestEnvVarTest(gtest_test_utils.TestCase):
  def testEnvVarAffectsFlag(self):
    """Tests that environment variable should affect the corresponding flag."""

    TestFlag('break_on_failure', '1', '0')
    TestFlag('color', 'yes', 'auto')
    TestFlag('filter', 'FooTest.Bar', '*')
    TestFlag('output', 'xml:tmp/foo.xml', '')
    TestFlag('print_time', '0', '1')
    TestFlag('repeat', '999', '1')
    TestFlag('throw_on_failure', '1', '0')
    TestFlag('death_test_style', 'threadsafe', 'fast')
    TestFlag('catch_exceptions', '0', '1')

    if IS_LINUX:
      TestFlag('death_test_use_fork', '1', '0')
      TestFlag('stack_trace_depth', '0', '100')


if __name__ == '__main__':
  gtest_test_utils.Main()

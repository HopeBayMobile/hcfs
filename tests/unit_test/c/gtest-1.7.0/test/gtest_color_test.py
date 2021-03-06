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

"""Verifies that Google Test correctly determines whether to use colors."""

__author__ = 'wan@google.com (Zhanyong Wan)'

import os
import gtest_test_utils


IS_WINDOWS = os.name = 'nt'

COLOR_ENV_VAR = 'GTEST_COLOR'
COLOR_FLAG = 'gtest_color'
COMMAND = gtest_test_utils.GetTestExecutablePath('gtest_color_test_')


def SetEnvVar(env_var, value):
  """Sets the env variable to 'value'; unsets it when 'value' is None."""

  if value is not None:
    os.environ[env_var] = value
  elif env_var in os.environ:
    del os.environ[env_var]


def UsesColor(term, color_env_var, color_flag):
  """Runs gtest_color_test_ and returns its exit code."""

  SetEnvVar('TERM', term)
  SetEnvVar(COLOR_ENV_VAR, color_env_var)

  if color_flag is None:
    args = []
  else:
    args = ['--%s=%s' % (COLOR_FLAG, color_flag)]
  p = gtest_test_utils.Subprocess([COMMAND] + args)
  return not p.exited or p.exit_code


class GTestColorTest(gtest_test_utils.TestCase):
  def testNoEnvVarNoFlag(self):
    """Tests the case when there's neither GTEST_COLOR nor --gtest_color."""

    if not IS_WINDOWS:
      self.assert_(not UsesColor('dumb', None, None))
      self.assert_(not UsesColor('emacs', None, None))
      self.assert_(not UsesColor('xterm-mono', None, None))
      self.assert_(not UsesColor('unknown', None, None))
      self.assert_(not UsesColor(None, None, None))
    self.assert_(UsesColor('linux', None, None))
    self.assert_(UsesColor('cygwin', None, None))
    self.assert_(UsesColor('xterm', None, None))
    self.assert_(UsesColor('xterm-color', None, None))
    self.assert_(UsesColor('xterm-256color', None, None))

  def testFlagOnly(self):
    """Tests the case when there's --gtest_color but not GTEST_COLOR."""

    self.assert_(not UsesColor('dumb', None, 'no'))
    self.assert_(not UsesColor('xterm-color', None, 'no'))
    if not IS_WINDOWS:
      self.assert_(not UsesColor('emacs', None, 'auto'))
    self.assert_(UsesColor('xterm', None, 'auto'))
    self.assert_(UsesColor('dumb', None, 'yes'))
    self.assert_(UsesColor('xterm', None, 'yes'))

  def testEnvVarOnly(self):
    """Tests the case when there's GTEST_COLOR but not --gtest_color."""

    self.assert_(not UsesColor('dumb', 'no', None))
    self.assert_(not UsesColor('xterm-color', 'no', None))
    if not IS_WINDOWS:
      self.assert_(not UsesColor('dumb', 'auto', None))
    self.assert_(UsesColor('xterm-color', 'auto', None))
    self.assert_(UsesColor('dumb', 'yes', None))
    self.assert_(UsesColor('xterm-color', 'yes', None))

  def testEnvVarAndFlag(self):
    """Tests the case when there are both GTEST_COLOR and --gtest_color."""

    self.assert_(not UsesColor('xterm-color', 'no', 'no'))
    self.assert_(UsesColor('dumb', 'no', 'yes'))
    self.assert_(UsesColor('xterm-color', 'no', 'auto'))

  def testAliasesOfYesAndNo(self):
    """Tests using aliases in specifying --gtest_color."""

    self.assert_(UsesColor('dumb', None, 'true'))
    self.assert_(UsesColor('dumb', None, 'YES'))
    self.assert_(UsesColor('dumb', None, 'T'))
    self.assert_(UsesColor('dumb', None, '1'))

    self.assert_(not UsesColor('xterm', None, 'f'))
    self.assert_(not UsesColor('xterm', None, 'false'))
    self.assert_(not UsesColor('xterm', None, '0'))
    self.assert_(not UsesColor('xterm', None, 'unknown'))


if __name__ == '__main__':
  gtest_test_utils.Main()

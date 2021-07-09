#!/usr/bin/python
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
import sys
import os
import datetime

from setuptools import setup, find_packages, Extension
from setuptools.command.test import test as TestCommand


class PyTest(TestCommand):

    def initialize_options(self):
        TestCommand.initialize_options(self)
        self.pytest_args = ['tests/unit_test/python']

    def finalize_options(self):
        TestCommand.finalize_options(self)
        self.test_args = []
        self.test_suite = True

    def run_tests(self):
        # import here, cause outside the eggs aren't loaded
        import pytest
        errno = pytest.main(self.pytest_args or '')
        sys.exit(errno)


setup(
    name='pyhcfs',
    version=os.getenv('VERSION_NUM', datetime.datetime.now().isoformat('T')),
    package_dir={'':'src'},
    packages=find_packages("src"),
    setup_requires=['cffi >= 1.1'],
    cffi_modules=['src/pyhcfs/pyhcfs_build.py:ffi'],
    install_requires=['cffi >= 1.1'],
    tests_require=['pytest >= 2.7.3'],
    cmdclass={'test': PyTest})

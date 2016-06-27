#!/usr/bin/python
# vim: set fileencoding=UTF-8 :
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract: build & test script for hcfs's python lib
#
# Revision History
#   2016/6/16 Add header and encoding comments
#
##########################################################################
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

# -*- coding: utf-8 -*-
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
"""
    flask.testsuite.deprecations
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Tests deprecation support.

    :copyright: (c) 2011 by Armin Ronacher.
    :license: BSD, see LICENSE for more details.
"""

import flask
import unittest
from flask.testsuite import FlaskTestCase, catch_warnings


class DeprecationsTestCase(FlaskTestCase):
    """not used currently"""


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(DeprecationsTestCase))
    return suite

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
    flask.testsuite.subclassing
    ~~~~~~~~~~~~~~~~~~~~~~~~~~~

    Test that certain behavior of flask can be customized by
    subclasses.

    :copyright: (c) 2011 by Armin Ronacher.
    :license: BSD, see LICENSE for more details.
"""
import flask
import unittest
from logging import StreamHandler
from flask.testsuite import FlaskTestCase
from flask._compat import StringIO


class FlaskSubclassingTestCase(FlaskTestCase):

    def test_suppressed_exception_logging(self):
        class SuppressedFlask(flask.Flask):
            def log_exception(self, exc_info):
                pass

        out = StringIO()
        app = SuppressedFlask(__name__)
        app.logger_name = 'flask_tests/test_suppressed_exception_logging'
        app.logger.addHandler(StreamHandler(out))

        @app.route('/')
        def index():
            1 // 0

        rv = app.test_client().get('/')
        self.assert_equal(rv.status_code, 500)
        self.assert_in(b'Internal Server Error', rv.data)

        err = out.getvalue()
        self.assert_equal(err, '')


def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(FlaskSubclassingTestCase))
    return suite

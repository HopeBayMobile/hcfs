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
    flask.module
    ~~~~~~~~~~~~

    Implements a class that represents module blueprints.

    :copyright: (c) 2011 by Armin Ronacher.
    :license: BSD, see LICENSE for more details.
"""

import os

from .blueprints import Blueprint


def blueprint_is_module(bp):
    """Used to figure out if something is actually a module"""
    return isinstance(bp, Module)


class Module(Blueprint):
    """Deprecated module support.  Until Flask 0.6 modules were a different
    name of the concept now available as blueprints in Flask.  They are
    essentially doing the same but have some bad semantics for templates and
    static files that were fixed with blueprints.

    .. versionchanged:: 0.7
       Modules were deprecated in favor for blueprints.
    """

    def __init__(self, import_name, name=None, url_prefix=None,
                 static_path=None, subdomain=None):
        if name is None:
            assert '.' in import_name, 'name required if package name ' \
                'does not point to a submodule'
            name = import_name.rsplit('.', 1)[1]
        Blueprint.__init__(self, name, import_name, url_prefix=url_prefix,
                           subdomain=subdomain, template_folder='templates')

        if os.path.isdir(os.path.join(self.root_path, 'static')):
            self._static_folder = 'static'

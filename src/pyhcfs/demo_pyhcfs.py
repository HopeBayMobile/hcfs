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

import pprint
import textwrap
from parser import *

pp = pprint.PrettyPrinter(indent=2)
ret = { 'offset': (0, 0) }

class DemoPyHCFS:
    def demo_title(self, title):
        print("")
        print("Demo", title)
        print("==============================")

    @staticmethod
    def demo(cmd):
        prefix="    "
        print("\n" + prefix + cmd + "\n")
        exec('print(textwrap.indent(pp.pformat('+cmd+'), prefix))')

    def demoAll(self, test_target):
        self.demo_title("list_volume")
        self.demo('list_volume(b"'+test_target+'/fsmgr")')

        self.demo_title("list_volume (Failure)")
        self.demo('list_volume(b"")')

        self.demo_title("parse_meta")
        self.demo('parse_meta(b"'+test_target+'/meta_isdir")')

        self.demo_title("parse_meta (Failure)")
        self.demo('parse_meta(b"test_data/v0/android/meta_isreg")')

        self.demo_title("list_dir_inorder")
        self.demo('list_dir_inorder(b"'+test_target+'/meta_isdir", ret["offset"], limit=100)')

        self.demo_title("get_vol_usage")
        self.demo('get_vol_usage(b"'+test_target+'/FSstat")')

        self.demo_title("list_file_blocks")
        self.demo('list_file_blocks(b"'+test_target+'/meta_isreg")')

demoer = DemoPyHCFS()
demoer.demoAll('test_data/v1/android')
demoer.demoAll('test_data/v2/android')
demoer.demo('list_volume(b"'+'test_data/v0/android'+'/fsmgr")')
demoer.demo('list_volume(b"'+'test_data/v1/android'+'/fsmgr")')
demoer.demo('list_volume(b"'+'test_data/v2/android'+'/fsmgr")')

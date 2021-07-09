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
import os
import ast

from Case import Case
from .. import config
from Utils.metaParserAdapter import PyhcfsAdapter as pyhcfs
from Utils.FuncSpec import FuncSpec
from Utils.tedUtils import list_abspathes_filter_name, not_startswith
from constant import Path


class NormalCase(Case):
    """
    test_hcfs_list_external_volume_NormalFsmgr:
          1.Call API with normal fsmgr file
          2.(Expected) Result matches  with API input and normal output spec
          3.(Expected) Inode should match with stat inode
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup list_external_volume spec")
        # list_external_volume(b"test_data/v1/android/fsmgr")
        # normal : [(128, b'hcfs_external')]
        # error : -1
        # stderr : Error: list_external_volume: No such file or directory
        self.func_spec_verifier = FuncSpec([str], [[(int, str)]], [int])

    def test(self):
        result = pyhcfs.list_external_volume(Path.FSMGR_FILE)
        expected = self.get_fsmgr_stat()
        isPass, msg = self.func_spec_verifier.check_onNormal(
            [Path.FSMGR_FILE], [result])
        return isPass, msg
        if result[0][0] != expected["stat"]["ino"]:
            return False, "External volume doesn't match with expected in inode number"

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def get_fsmgr_stat(self):  # TODO : multiple external volume
        with open(Path.FSMGR_STAT, "rt") as fin:
            return ast.literal_eval(fin.read())


# inheritance NormalFsmgrCase(setUp, tearDown)
class RandomFileContentCase(NormalCase):
    """
    test_hcfs_list_external_volume_random_content_file:
          1.Call API with random content file path(FSstat, meta, data block, empty, random content)
          2.(Expected) Result matches  with API input and error output spec
          3.(Expected) Result code must be -1
    """

    def test(self):
        for path in list_abspathes_filter_name(Path.TEST_RANDOM_DIR, not_startswith("FSmgr")):
            result = pyhcfs.list_external_volume(path)
            isPass, msg = self.func_spec_verifier.check_onErr([path], [result])
            if not isPass:
                return False, msg
            if result >= 0:
                return False, "Result must be less than 0"
        return True, ""


# inheritance NormalFsmgrCase(setUp, tearDown)
class NonexistedAndEmptyPathCase(NormalCase):
    """
    test_hcfs_parse_fsmgr_NonexistedAndEmptyPath:
          1.Call API with non-existed and empty file path
          2.(Expected) Result matches  with API input and error output spec
          3.(Expected) Result code must be -1
    """

    def test(self):
        nonexisted_path = ["/no/such/", "/no/such/fsmgr", "/and/directory", ""]
        for path in nonexisted_path:
            result = pyhcfs.list_external_volume(path)
            isPass, msg = self.func_spec_verifier.check_onErr([path], [result])
            if not isPass:
                return False, msg
            if result >= 0:
                return False, "Result must be less than 0"
        return True, ""

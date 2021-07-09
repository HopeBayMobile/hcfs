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
from Utils.tedUtils import list_abspathes_filter_name, not_startswith, file_name
from constant import FileType, Path


class NormalCase(Case):
    """
    test_hcfs_list_file_blocks_NormalMetaPath:
          1.Call API with normal meta file path
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result code must be 0
          4.(Expected) Result blocks length should equal to ret_num
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup list_file_blocks spec")
        # list_file_blocks(b"test_data/v1/android/meta_isreg")
        # { 'block_list': [ 'data_6086_10240_1',
        #                   'data_6086_10241_1',
        #                   'data_6086_10242_1',
        #                   'data_6086_10243_1'],
        #   'result': 0,
        #   'ret_num': 4}
        self.func_spec_verifier = FuncSpec(
            [str], [{"block_list": [str], "result":int, "ret_num":int}])

    def test(self):
        for meta_path in self.get_file_meta_pathes():
            result = pyhcfs.list_file_blocks(meta_path)
            isPass, msg = self.func_spec_verifier.check_onNormal([meta_path], [
                                                                 result])
            if not isPass:
                return False, msg
            if result["result"] != 0:
                return False, "Result should be 0" + repr(meta_path, result)
            if len(result["block_list"]) != result["ret_num"]:
                return False, "Result blocks length should equal to ret_num:" + str(result)
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def get_file_meta_pathes(self):
        for path in list_abspathes_filter_name(Path.TEST_DATA_DIR, str.isdigit):
            ino = file_name(path)
            if self.get_file_type(path, ino) == FileType.FILE:  # file or TODO socket???
                yield os.path.join(path, "meta_" + ino)

    def get_file_type(self, path, ino):
        stat_path = os.path.join(path, ino)
        with open(stat_path, "rt") as file:
            stat = ast.literal_eval(file.read())
            return stat["file_type"]


# inheritance NormalCase(setUp, tearDown)
class RandomFileContentCase(NormalCase):
    """
    test_hcfs_list_file_blocks_random_content_file:
          1.Call API with random content file path(fsmgr, FSstat, non-file meta, data block, empty, random content)
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result code must be  less than 0
    """

    def test(self):
        for path in self.get_random_test_data():
            result = pyhcfs.list_file_blocks(path)
            isPass, msg = self.func_spec_verifier.check_onNormal([path], [
                                                                 result])
            if not isPass:
                return False, msg
            if result["result"] >= 0:
                return False, "Result should be less than 0:" + str(result)
        return True, ""

    def get_random_test_data(self):
        random_data_pathes = list_abspathes_filter_name(
            Path.TEST_RANDOM_DIR, not_startswith("meta"))
        return list(random_data_pathes) + self.get_non_file_meta_pathes()

    def get_non_file_meta_pathes(self):
        path_ino_pairs = [(path, path.split("/")[-1])
                          for path in list_abspathes_filter_name(Path.TEST_DATA_DIR, str.isdigit)]
        return [os.path.join(path, "meta_" + ino) for path, ino in path_ino_pairs if self.get_file_type(path, ino) != FileType.FILE]


# inheritance NormalCase(setUp, tearDown)
class NonExistedAndEmptyPathCase(NormalCase):
    """
    test_hcfs_list_file_blocks_NonexistedAndEmptyPath:
        1.Call API with non-existed and empty file path.
        2.(Expected) Result matched with API input and normal output spec
        3.(Expected) Result code must be -1
    """

    def test(self):
        nonexisted_path = ["/no/such/", "/no/such/file", "/and/directory", ""]
        for path in nonexisted_path:
            result = pyhcfs.list_file_blocks(path)
            isPass, msg = self.func_spec_verifier.check_onNormal([path], [
                                                                 result])
            if not isPass:
                return False, msg
            if result["result"] != -1:
                return False, "Result should be -1:" + str(result)
        return True, ""

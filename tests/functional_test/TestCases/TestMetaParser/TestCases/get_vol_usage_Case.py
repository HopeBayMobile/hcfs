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

from Case import Case
from .. import config
from Utils.metaParserAdapter import PyhcfsAdapter as pyhcfs
from Utils.FuncSpec import FuncSpec
from Utils.tedUtils import list_abspathes_filter_name, not_startswith, startswith, file_name
from constant import Path


class NormalCase(Case):
    """
    test_hcfs_get_vol_usage_normal_FSstat:
          1.Call API with normal FSstat file path
          2.(Expected) Result matches with API input and normal output spec
          3.(Expected) Result code must be 0
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup get_vol_usage spec")
        # get_vol_usage(b"test_data/v1/android/FSstat")
        # {'result': 0, 'usage': 1373381904}
        self.func_spec_verifier = FuncSpec(
            [str], [{"result": int, "usage": int}])

    def test(self):
        for path in list_abspathes_filter_name(Path.TEST_DATA_DIR, startswith("FSstat")):
            result = pyhcfs.get_vol_usage(path)
            isPass, msg = self.func_spec_verifier.check_onNormal([path], [
                                                                 result])
            if not isPass:
                return False, msg
            if result["result"] != 0:
                return False, "Result code should be 0" + repr(result)
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")


# inheritance NormalCase(setUp, tearDown)
class RandomFileContentCase(NormalCase):
    """
    test_hcfs_get_vol_usage_random_content_file:
          1.Call API with random content file path(fsmgr, meta, data block, empty, random content)
          2.(Expected) Result matches with API input and normal output spec
          3.(Expected) Result code must be -1
    """

    def test(self):
        for path in list_abspathes_filter_name(Path.TEST_RANDOM_DIR, not_startswith("FSstat")):
            result = pyhcfs.get_vol_usage(path)
            isPass, msg = self.func_spec_verifier.check_onNormal([path], [
                                                                 result])
            if not isPass:
                return False, msg
            if result["result"] != -1:
                return False, "Result should be -1" + str(result)
        return True, ""


# inheritance NormalCase(setUp, tearDown)
class NonExistedAndEmptyPathCase(NormalCase):
    """
    test_hcfs_get_vol_usage_NonexistedAndEmptyPath:
        1.Call API with non-existed and empty file path.
        2.(Expected) Result matches with API input and normal output spec
        3.(Expected) Result code must be -1
    """

    def test(self):
        nonexisted_path = ["/no/such/", "/no/such/file", "/and/directory", ""]
        for path in nonexisted_path:
            result = pyhcfs.get_vol_usage(path)
            isPass, msg = self.func_spec_verifier.check_onNormal([path], [
                                                                 result])
            if not isPass:
                return False, msg
            if result["result"] != -1:
                return False, "Result should be -1" + str(result)
        return True, ""

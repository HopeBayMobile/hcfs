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
import shutil

import config
from TestCases import *

logger = config.get_logger().getChild(__name__)

THIS_DIR = os.path.abspath(os.path.dirname(__file__))


class TestMetaParser_00(object):
    """
    Global setUp
    Do nothing
    """

    def run(self):
        logger.info("Global setup")
        path = os.path.join(THIS_DIR, "report")
        logger.info("Remove report directory")
        if os.path.exists(path):
            shutil.rmtree(path)
        return True, ""


class TestMetaParser_99(object):
    """
    Global tearDown
    Do nothing
    """

    def run(self):
        logger.info("Global tearDown")
        return True, "Do nothing"


class TestMetaParser_117001(list_external_volume_Case.NormalCase):
    pass


class TestMetaParser_117002(list_external_volume_Case.RandomFileContentCase):
    pass


class TestMetaParser_117003(list_external_volume_Case.NonexistedAndEmptyPathCase):
    pass


class TestMetaParser_117004(parse_meta_Case.NormalCase):
    pass


class TestMetaParser_117005(parse_meta_Case.RandomFileContentCase):
    pass


class TestMetaParser_117006(parse_meta_Case.NonexistedAndEmptyPathCase):
    pass


class TestMetaParser_117007(list_dir_inorder_Case.NormalCase):
    pass


class TestMetaParser_117008(list_dir_inorder_Case.RandomFileContentCase):
    pass


class TestMetaParser_117009(list_dir_inorder_Case.NormalMetaPathLimitInvalidOffsetCase):
    pass


class TestMetaParser_117010(list_dir_inorder_Case.NormalMetaPathOffsetInvalidLimitCase):
    pass


class TestMetaParser_117011(list_dir_inorder_Case.NormalMetaPathInvalidOffsetLimitCase):
    pass


class TestMetaParser_117012(list_dir_inorder_Case.NonexistedAndEmptyPathCase):
    pass


class TestMetaParser_117013(get_vol_usage_Case.NormalCase):
    pass


class TestMetaParser_117014(get_vol_usage_Case.RandomFileContentCase):
    pass


class TestMetaParser_117015(get_vol_usage_Case.NonExistedAndEmptyPathCase):
    pass


class TestMetaParser_117016(list_file_blocks_Case.NormalCase):
    pass


class TestMetaParser_117017(list_file_blocks_Case.RandomFileContentCase):
    pass


class TestMetaParser_117018(list_file_blocks_Case.NonExistedAndEmptyPathCase):
    pass


class TestMetaParser_117019(behaviorCase.ExternalVolHasFSstatCase):
    pass


class TestMetaParser_117020(behaviorCase.NonexistMetaPathCase):
    pass


class TestMetaParser_117021(behaviorCase.test_hcfs_behavior_FSstat_exists):
    pass

if __name__ == '__main__':
    TestMetaParser_00().run()
    TestMetaParser_01().run()
    TestMetaParser_02().run()
    TestMetaParser_03().run()
    TestMetaParser_04().run()
    TestMetaParser_05().run()
    TestMetaParser_06().run()
    TestMetaParser_07().run()
    TestMetaParser_08().run()
    TestMetaParser_09().run()
    TestMetaParser_10().run()
    TestMetaParser_11().run()
    TestMetaParser_12().run()
    TestMetaParser_99().run()

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


class TestMetaParser_117002(list_external_volume_Case.NonexistedAndEmptyPathCase):
    pass


class TestMetaParser_117003(parse_meta_Case.NormalCase):
    pass


class TestMetaParser_117004(parse_meta_Case.NonexistedAndEmptyPathCase):
    pass


class TestMetaParser_117005(list_dir_inorder_Case.NormalCase):
    pass


class TestMetaParser_117006(list_dir_inorder_Case.NormalMetaPathLimitInvalidOffsetCase):
    pass


class TestMetaParser_117007(list_dir_inorder_Case.NormalMetaPathOffsetInvalidLimitCase):
    pass


class TestMetaParser_117008(list_dir_inorder_Case.NormalMetaPathInvalidOffsetLimitCase):
    pass


class TestMetaParser_117009(list_dir_inorder_Case.NonexistedAndEmptyPathCase):
    pass


class TestMetaParser_117010(get_vol_usage_Case.NormalCase):
    pass


class TestMetaParser_117011(get_vol_usage_Case.RandomFileContentCase):
    pass


class TestMetaParser_117012(get_vol_usage_Case.NonExistedAndEmptyPathCase):
    pass


class TestMetaParser_117013(list_file_blocks_Case.NormalCase):
    pass


class TestMetaParser_117014(list_file_blocks_Case.RandomFileContentCase):
    pass


class TestMetaParser_117015(list_file_blocks_Case.NonExistedAndEmptyPathCase):
    pass


class TestMetaParser_117016(behaviorCase.ExternalVolHasFSstatCase):
    pass


class TestMetaParser_117017(behaviorCase.NonexistMetaPathCase):
    pass


class TestMetaParser_117018(behaviorCase.test_hcfs_behavior_FSstat_exists):
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

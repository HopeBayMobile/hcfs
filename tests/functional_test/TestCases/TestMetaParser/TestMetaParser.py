import os
import time
import ast
from itertools import product

import config
from TestCases import *

logger = config.get_logger().getChild(__name__)


class TestMetaParser_00(object):
    """
    Global setUp
    Do nothing
    """

    def run(self):
        logger.info("Global setup")
        return True, "Do nothing"


class TestMetaParser_99(object):
    """
    Global tearDown
    Do nothing
    """

    def run(self):
        logger.info("Global tearDown")
        return True, "Do nothing"


class TestMetaParser_117001(list_external_volume_Case.NormalFsmgrCase):
    pass


class TestMetaParser_117002(list_external_volume_Case.NonexistFsmgrPathCase):
    pass


class TestMetaParser_117003(list_external_volume_Case.EmptyFsmgrPathCase):
    pass


class TestMetaParser_117004(parse_meta_Case.NormalMetaPathCase):
    pass


class TestMetaParser_117005(parse_meta_Case.NonexistMetaPathCase):
    pass


class TestMetaParser_117006(parse_meta_Case.EmptyMetaPathCase):
    pass


class TestMetaParser_117007(list_dir_inorder_Case.NormalMetaPathCase):
    pass


class TestMetaParser_117008(list_dir_inorder_Case.NormalMetaPathLimitInvalidOffsetCase):
    pass


class TestMetaParser_117009(list_dir_inorder_Case.NormalMetaPathOffsetInvalidLimitCase):
    pass


class TestMetaParser_117010(list_dir_inorder_Case.NormalMetaPathInvalidOffsetLimitCase):
    pass


class TestMetaParser_117011(list_dir_inorder_Case.NonexistMetaPathCase):
    pass


class TestMetaParser_117012(list_dir_inorder_Case.EmptyMetaPathCase):
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

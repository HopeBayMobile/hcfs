import os

from Case import Case
import config
from Utils.metaParserAdapter import *
from Utils.FuncSpec import FuncSpec

# Test config, do not change these value during program.
# These vars are final in term of Java.
TEST_DATA_DIR = os.path.abspath(os.path.dirname(__file__)) + "/test_data"
FSMGR_FILE = TEST_DATA_DIR + "/fsmgr"
################## test config ##################
# TODO empty fsmgr file content
# TODO random fsmgr file content


class NormalFsmgrCase(Case):
    """
    test_hcfs_parse_fsmgr_NormalFsmgr:
          1.Call API with normal fsmgr file
          2.(Expected) Result matched with API input and normal output spec
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup list_external_volume spec")
        self.list_external_volume_spec = FuncSpec([str], [[(int, str)]], [int])

    def test(self):
        result = list_external_volume(FSMGR_FILE)
        isPass, msg = self.list_external_volume_spec.check_onNormal([FSMGR_FILE], [
            result])
        return isPass, msg

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")


# inheritance NormalFsmgrCase(setUp, tearDown)
class NonexistFsmgrPathCase(NormalFsmgrCase):
    """
    test_hcfs_parse_fsmgr_NonexistFsmgrPath:
          1.Call API with non-existed fsmgr file path
          2.(Expected) Result matched with API input and error output spec
          3.(Expected) Result code must be -1
    """

    def test(self):
        nonexisted_path = ["/no", "/such/", "/no/such/fsmgr", "/and/directory"]
        for path in nonexisted_path:
            result = list_external_volume(path)
            isPass, msg = self.list_external_volume_spec.check_onErr([path], [
                result])
            if not isPass:
                return False, msg
            if result != -1:
                return False, "Result code should be -1"
        return True, ""


# inheritance NormalFsmgrCase(setUp, tearDown)
class EmptyFsmgrPathCase(NormalFsmgrCase):
    """
    test_hcfs_parse_fsmgr_EmptyFsmgrPath:
        1.Call API with empty fsmgr file path
        2.(Expected) Result matched with API input and error output spec
        3.(Expected) Result code must be -1
    """

    def test(self):
        result = list_external_volume("")
        isPass, msg = self.list_external_volume_spec.check_onErr([""], [
                                                                 result])
        if not isPass:
            return False, msg
        if result != -1:
            return False, "Result code should be -1"
        return True, ""

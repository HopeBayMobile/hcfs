import os

from Case import Case
import config
from Utils.metaParserAdapter import *
from Utils.FuncSpec import FuncSpec
from Utils.log import LogFile

# Test config, do not change these value during program.
# These vars are final in term of Java.
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
TEST_DATA_DIR = os.path.join(THIS_DIR, "test_data_v2")
FSMGR_FILE = os.path.join(TEST_DATA_DIR, "fsmgr")
FSMGR_STAT = os.path.join(TEST_DATA_DIR, "fsmgr_stat")
REPORT_DIR = os.path.join(THIS_DIR, "..", "report")
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
        self.log_file = LogFile(REPORT_DIR, "list_external_volume")
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup list_external_volume spec")
        # list_external_volume(b"test_data/v1/android/fsmgr")
        # normal : [(128, b'hcfs_external')]
        # error : -1
        # stderr : Error: list_external_volume: No such file or directory
        self.list_external_volume_spec = FuncSpec([str], [[(int, str)]], [int])

    def test(self):
        result = list_external_volume(FSMGR_FILE)
        self.log_file.recordFunc("list_external_volume", FSMGR_FILE, result)
        expected = self.get_fsmgr_stat()
        if result[0][0] != expected["stat"]["ino"]:
            return False, "External volume doesn't match with expected in inode number"
        isPass, msg = self.list_external_volume_spec.check_onNormal([FSMGR_FILE], [
            result])
        return isPass, msg

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def get_fsmgr_stat(self):
        with open(FSMGR_STAT, "rt") as fin:
            return ast.literal_eval(fin.read())


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
            self.log_file.recordFunc("list_external_volume", path, result)
            isPass, msg = self.list_external_volume_spec.check_onErr([path], [
                result])
            if not isPass:
                return False, msg
            if result >= 0:
                return False, "Result must be less than 0"
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
        self.log_file.recordFunc("list_external_volume", "", result)
        isPass, msg = self.list_external_volume_spec.check_onErr([""], [
                                                                 result])
        if not isPass:
            return False, msg
        if result >= 0:
            return False, "Result must be less than 0"
        return True, ""

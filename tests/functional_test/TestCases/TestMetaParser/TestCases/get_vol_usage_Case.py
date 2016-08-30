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
REPORT_DIR = os.path.join(THIS_DIR, "..", "report")
################## test config ##################


class NormalCase(Case):
    """
    test_hcfs_get_vol_usage_normal_FSstat:
          1.Call API with normal FSstat file path
          2.(Expected) Result matches with API input and normal output spec
          3.(Expected) Result code must be 0
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.log_file = LogFile(REPORT_DIR, "get_vol_usage")
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup get_vol_usage spec")
        self.get_vol_usage_spec = FuncSpec(
            [str], [{"result": int, "usage": int}])

    def test(self):
        for fsstat_path in self.get_fsstat():
            result = get_vol_usage(fsstat_path)
            self.log_file.recordFunc("get_vol_usage", fsstat_path, result)

            isPass, msg = self.get_vol_usage_spec.check_onNormal(
                [fsstat_path], [result])
            if not isPass:
                return False, msg
            if result["result"] != 0:
                return False, "Result code should be 0" + repr(result)
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def get_fsstat(self):
        for root_dir, names, _ in os.walk(TEST_DATA_DIR):
            for name in (x for x in names if x.startswith("FSstat")):
                yield os.path.join(root_dir, name)


# inheritance NormalCase(setUp, tearDown)
class RandomFSstatContentCase(NormalCase):
    """
    test_hcfs_get_vol_usage_random_content_file:
          1.Call API with random content file path(fsmgr, meta, data block, empty, random content)
          2.(Expected) Result matches with API input and normal output spec
          3.(Expected) Result code must be -1
    """

    def test(self):
        random_data_dir = os.path.join(TEST_DATA_DIR, "random")
        for file_name in os.listdir(random_data_dir):
            test_file_path = os.path.join(random_data_dir, file_name)
            result = get_vol_usage(test_file_path)
            self.log_file.recordFunc("get_vol_usage", test_file_path, result)
            isPass, msg = self.get_vol_usage_spec.check_onNormal(
                [test_file_path], [result])
            if not isPass:
                return False, msg
            if result["result"] != -1:
                return False, "Result should be -1" + str(result)
        return True, ""


# inheritance NormalCase(setUp, tearDown)
class NonExistedPathCase(NormalCase):
    """
    test_hcfs_get_vol_usage_NonexistedPath:
        1.Call API with non-existed file path.
        2.(Expected) Result matches with API input and normal output spec
        3.(Expected) Result code must be -1
    """

    def test(self):
        nonexisted_path = ["/no", "/such/", "/no/such/file", "/and/directory"]
        for path in nonexisted_path:
            result = get_vol_usage(path)
            self.log_file.recordFunc("get_vol_usage", path, result)
            isPass, msg = self.get_vol_usage_spec.check_onNormal([path], [
                                                                 result])
            if not isPass:
                return False, msg
            if result["result"] != -1:
                return False, "Result should be -1" + str(result)
        return True, ""


# inheritance NormalCase(setUp, tearDown)
class EmptyPathCase(NormalCase):
    """
    test_hcfs_get_vol_usage_EmptyPath:
        1.Call API with empty sfile path.
        2.(Expected) Result matches with API input and normal output spec
        3.(Expected) Result code must be -1
    """

    def test(self):
        result = get_vol_usage("")
        self.log_file.recordFunc("get_vol_usage", "", result)
        isPass, msg = self.get_vol_usage_spec.check_onNormal([""], [result])
        if not isPass:
            return False, msg
        if result["result"] != -1:
            return False, "Result should be -1" + str(result)
        return True, ""

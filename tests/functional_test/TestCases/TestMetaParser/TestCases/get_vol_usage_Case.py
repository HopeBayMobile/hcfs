import os

from Case import Case
import config
from Utils.metaParserAdapter import pyhcfs
from Utils.FuncSpec import FuncSpec
from Utils.tedUtils import listdir_full, listdir_path, negate
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
        for path in listdir_path(Path.TEST_DATA_DIR, str.startswith, ("FSstat",)):
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
        notstartswith = negate(str.startswith)
        for path in listdir_path(Path.TEST_RANDOM_DIR, notstartswith, ("FSstat",)):
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

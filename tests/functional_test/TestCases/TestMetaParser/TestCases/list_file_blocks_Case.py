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
    test_hcfs_list_file_blocks_NormalMetaPath:
          1.Call API with normal meta file path
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result code must be 0
          4.(Expected) Result blocks length should equal to ret_num
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.log_file = LogFile(REPORT_DIR, "list_file_blocks")
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
        self.list_file_blocks_spec = FuncSpec(
            [str], [{"block_list": [str], "result":int, "ret_num":int}])

    def test(self):
        for meta_path in self.get_file_meta_pathes():
            result = list_file_blocks(meta_path)
            self.log_file.recordFunc("list_file_blocks", meta_path, result)
            isPass, msg = self.list_file_blocks_spec.check_onNormal(
                [meta_path], [result])
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
        # test_data
        #  |  12/meta_12   (meta)
        #  |  12/12             (stat)
        for dir_path, dir_names, _ in os.walk(TEST_DATA_DIR):
            for name in (x for x in dir_names if x.isdigit()):
                stat = self.get_stat(os.path.join(dir_path, name, name))
                if stat["file_type"] == 1:  # file or TODO socket???
                    yield os.path.abspath(os.path.join(dir_path, name, "meta_" + name))

    def get_stat(self, path):
        with open(path, "rt") as fin:
            return ast.literal_eval(fin.read())


# inheritance NormalCase(setUp, tearDown)
class RandomFileContentCase(NormalCase):
    """
    test_hcfs_list_file_blocks_random_content_file:
          1.Call API with random content file path(fsmgr, FSstat, non-file meta, data block, empty, random content)
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result code must be  less than 0
    """

    def test(self):
        random_data_dir = os.path.join(TEST_DATA_DIR, "random")
        for file_name in os.listdir(random_data_dir):
            test_file_path = os.path.join(random_data_dir, file_name)
            result = list_file_blocks(test_file_path)
            self.log_file.recordFunc(
                "list_file_blocks", test_file_path, result)
            isPass, msg = self.list_file_blocks_spec.check_onNormal(
                [test_file_path], [result])
            if not isPass:
                return False, msg
            if result["result"] >= 0:
                return False, "Result should be less than 0:" + str(result)
        return True, ""


# inheritance NormalCase(setUp, tearDown)
class NonExistedPathCase(NormalCase):
    """
    test_hcfs_list_file_blocks_NonexistlFSstatPath:
        1.Call API with non-existed file path.
        2.(Expected) Result matched with API input and normal output spec
        3.(Expected) Result code must be -1
    """

    def test(self):
        nonexisted_path = ["/no", "/such/", "/no/such/file", "/and/directory"]
        for path in nonexisted_path:
            result = list_file_blocks(path)
            self.log_file.recordFunc("list_file_blocks", path, result)
            isPass, msg = self.list_file_blocks_spec.check_onNormal([path], [
                                                                    result])
            if not isPass:
                return False, msg
            if result["result"] != -1:
                return False, "Result should be -1:" + str(result)
        return True, ""


# inheritance NormalCase(setUp, tearDown)
class EmptyPathCase(NormalCase):
    """
    test_hcfs_list_file_blocks_EmptyFSstatPath:
        1.Call API with empty file path
        2.(Expected) Result matched with API input and normal output spec
        3.(Expected) Result code must be -1
    """

    def test(self):
        result = list_file_blocks("")
        self.log_file.recordFunc("list_file_blocks", "", result)
        isPass, msg = self.list_file_blocks_spec.check_onNormal([""], [result])
        if not isPass:
            return False, msg
        if result["result"] != -1:
            return False, "Result should be -1:" + str(result)
        return True, ""

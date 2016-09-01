import os

from Case import Case
import config
from Utils.metaParserAdapter import *
from Utils.FuncSpec import FuncSpec
from Utils.tedUtils import listdir_full, listdir_path, negate
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
        self.func_spec = FuncSpec(
            [str], [{"block_list": [str], "result":int, "ret_num":int}])

    def test(self):
        for meta_path in self.get_file_meta_pathes():
            result = list_file_blocks(meta_path)
            isPass, msg = self.func_spec.check_onNormal([meta_path], [result])
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
        for path, ino in listdir_full(Path.TEST_DATA_DIR, str.isdigit):
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
            result = list_file_blocks(path)
            isPass, msg = self.func_spec.check_onNormal([path], [result])
            if not isPass:
                return False, msg
            if result["result"] >= 0:
                return False, "Result should be less than 0:" + str(result)
        return True, ""

    def get_random_test_data(self):
        notstartswith = negate(str.startswith)
        random_data_pathes = listdir_path(
            Path.TEST_RANDOM_DIR, notstartswith, ("meta",))
        return list(random_data_pathes) + self.get_non_file_meta_pathes()

    def get_non_file_meta_pathes(self):
        path_ino_pairs = [(path, ino)
                          for path, ino in listdir_full(Path.TEST_DATA_DIR, str.isdigit)]
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
            result = list_file_blocks(path)
            isPass, msg = self.func_spec.check_onNormal([path], [result])
            if not isPass:
                return False, msg
            if result["result"] != -1:
                return False, "Result should be -1:" + str(result)
        return True, ""

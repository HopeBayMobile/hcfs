import os
from itertools import product

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

# (0,0)
# TODO find valid offset
VALID_OFFSET = [(0, 0)]
INVALID_OFFSET = [(0, -1), (0, -101), (-1, 0), (1, -1),
                  (23, -1), (-1, 101), (-23, -101)]
# TODO: rand number
# TODO: 0 case
# 1~1000
VALID_LIMIT = [1, 10, 100, 1000]
INVALID_LIMIT = [0, -1, -10, 1001, 1100]
################## test config ##################
# TODO valid file meta path should be failed
# TODO empty meta file content
# TODO random meta file content
# TODO child list element attribute d_type???


class NormalCase(Case):
    """
    test_hcfs_list_dir_inorder_NormalMetaPath:
          1.Call API with normal meta file path, valid offset and valid limit
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result offset must be both positive number
          4.(Expected) Result code must be positive number
          5.(Expected) Result child list must be in name order
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.log_file = LogFile(REPORT_DIR, "list_dir_inorder")
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup list_dir_inorder spec")
        # list_dir_inorder(b"test_data/v1/android/meta_isdir", ret["offset"], limit=100)
        # { 'child_list': [ {'d_name': b'.', 'd_type': 0, 'inode': 6088}, ...], 'num_child_walked': 100, 'offset': (83560, 9), 'result': 0}
        # { 'child_list': [], 'num_child_walked': 0, 'offset': (0, 0), 'result': -1}
        output_spec_str = {"result": int, "offset": (int, int),
                           "child_list": [{"d_name": str, "inode": int, "d_type": int}],
                           "num_child_walked": int}
        self.list_dir_inorder_spec = FuncSpec(
            [str, (int, int), int], [output_spec_str])

    def test(self):
        normal_meta_path = self.get_dir_meta_pathes()
        for args in product(normal_meta_path, VALID_OFFSET, VALID_LIMIT):
            result = list_dir_inorder(*args)
            self.log_file.recordFunc("list_dir_inorder", args, result)
            isPass, msg = self.list_dir_inorder_spec.check_onNormal(list(args), [
                result])
            if not isPass:
                return False, msg
            if result["offset"] < (0, 0):
                return False, "Offset must be both positive num"
            if result["result"] < 0:
                return False, "Result must be positive num"
            if result["num_child_walked"] != len(result["child_list"]):
                return False, "Number of children doesn't match with 'num_child_walked'"
            if not self.expect_children_in_name_order(result["child_list"]):
                return False, "Child list is not in order"
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def get_dir_meta_pathes(self):
        # test_data
        #  |  12/meta_12   (meta)
        #  |  12/12             (stat)
        for dir_path, dir_names, _ in os.walk(TEST_DATA_DIR):
            for name in (x for x in dir_names if x.isdigit()):
                stat = self.get_stat(os.path.join(dir_path, name, name))
                if stat["file_type"] == 0:
                    yield os.path.abspath(os.path.join(dir_path, name, "meta_" + name))

    def get_stat(self, path):
        with open(path, "rt") as fin:
            return ast.literal_eval(fin.read())

    def expect_children_in_name_order(self, child_list):
        if len(child_list) == 0:
            return True
        self.isOrder = True
        reduce(self.name_order_checker, child_list)
        return self.isOrder

    def name_order_checker(self, pre_one, one):
        if pre_one["d_name"] > one["d_name"]:
            self.isOrder = False
        return one


# inheritance NormalMetaPathCase(setUp, tearDown)
class NormalMetaPathLimitInvalidOffsetCase(NormalCase):
    """
    test_hcfs_query_dir_list_NormalMetapathLimit_InvalidOffset:
          1.Call API with normal meta file path, invalid offset and valid limit
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result offset must be (0, 0)
          4.(Expected) Result code must be less than 0
          5.(Expected) Result child list must be empty
    """

    def test(self):
        normal_meta_path = self.get_dir_meta_pathes()
        for args in product(normal_meta_path, INVALID_OFFSET, VALID_LIMIT):
            result = list_dir_inorder(*args)
            self.log_file.recordFunc("list_dir_inorder", args, result)
            isPass, msg = self.list_dir_inorder_spec.check_onNormal(list(args), [
                result])
            if not isPass:
                return False, msg
            if result["offset"] != (0, 0):
                return False, "Offset must be (0, 0)"
            if result["result"] >= 0:
                return False, "Result must be less than 0"
            if result["num_child_walked"] != 0:
                return False, "'num_child_walked' must be 0"
            if len(result["child_list"]) != 0:
                return False, "Result child list must be empty"
        return True, ""


# inheritance NormalMetaPathCase(setUp, tearDown)
class NormalMetaPathOffsetInvalidLimitCase(NormalCase):
    """
    test_hcfs_query_dir_list_NormalMetaPathOffset_InvalidLimit:
          1.Call API with normal meta file path, valid offset and invalid limit
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result offset must be (0, 0)
          4.(Expected) Result code must be less than 0
          5.(Expected) Result child list must be empty
    """

    def test(self):
        normal_meta_path = self.get_dir_meta_pathes()
        for args in product(normal_meta_path, VALID_OFFSET, INVALID_LIMIT):
            result = list_dir_inorder(*args)
            self.log_file.recordFunc("list_dir_inorder", args, result)
            isPass, msg = self.list_dir_inorder_spec.check_onNormal(list(args), [
                result])
            if not isPass:
                return False, msg
            if result["offset"] != (0, 0):
                return False, "Offset must be (0, 0)"
            if result["result"] >= 0:
                return False, "Result must be less than 0"
            if result["num_child_walked"] != 0:
                return False, "'num_child_walked' must be 0"
            if len(result["child_list"]) != 0:
                return False, "Result child list must be empty"
        return True, ""


# inheritance NormalMetaPathCase(setUp, tearDown)
class NormalMetaPathInvalidOffsetLimitCase(NormalCase):
    """
    test_hcfs_query_dir_list_NormalMetaPath_InvalidOffsetLimit:
          1.Call API with normal meta file path, invalid offset and invalid limit
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result offset must be (0, 0)
          4.(Expected) Result code must be less than 0
          5.(Expected) Result child list must be empty
    """

    def test(self):
        normal_meta_path = self.get_dir_meta_pathes()
        for args in product(normal_meta_path, INVALID_OFFSET, INVALID_LIMIT):
            result = list_dir_inorder(*args)
            self.log_file.recordFunc("list_dir_inorder", args, result)
            isPass, msg = self.list_dir_inorder_spec.check_onNormal(list(args), [
                result])
            if not isPass:
                return False, msg
            if result["offset"] != (0, 0):
                return False, "Offset must be (0, 0)"
            if result["result"] >= 0:
                return False, "Result must be less than 0"
            if result["num_child_walked"] != 0:
                return False, "'num_child_walked' must be 0"
            if len(result["child_list"]) != 0:
                return False, "Result child list must be empty"
        return True, ""


# inheritance NormalMetaPathCase(setUp, tearDown)
class NonexistedAndEmptyPathCase(NormalCase):
    """
    test_hcfs_query_dir_list_NonexistedAndEmptyPath:
          1.Call API with non-existed adn empty file path, any offset and any limit
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result offset must be (0, 0)
          4.(Expected) Result code must be less than 0
          5.(Expected) Result child list must be empty
    """

    def test(self):
        normal_meta_path = ["/no", "/such", "/file/and/directory", ""]
        any_offset = VALID_OFFSET + INVALID_OFFSET
        any_limit = VALID_LIMIT + INVALID_LIMIT
        for args in product(normal_meta_path, any_offset, any_limit):
            result = list_dir_inorder(*args)
            self.log_file.recordFunc("list_dir_inorder", args, result)
            isPass, msg = self.list_dir_inorder_spec.check_onNormal(list(args), [
                result])
            if not isPass:
                return False, msg
            if result["offset"] != (0, 0):
                return False, "Offset must be (0, 0)"
            if result["result"] >= 0:
                return False, "Result must be less than 0"
            if result["num_child_walked"] != 0:
                return False, "'num_child_walked' must be 0"
            if len(result["child_list"]) != 0:
                return False, "Result child list must be empty"
        return True, ""

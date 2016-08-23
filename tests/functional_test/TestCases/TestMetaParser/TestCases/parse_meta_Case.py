import os

from Case import Case
import config
from Utils.metaParserAdapter import *
from Utils.FuncSpec import FuncSpec

# Test config, do not change these value during program.
# These vars are final in term of Java.
TEST_DATA_DIR = os.path.abspath(os.path.dirname(__file__)) + "/test_data"
EXPECTED_ERR = {'file_type': 0,
                'stat': {'size': 0, 'blocks': 0, 'uid': 0, 'mtime_nsec': 0, 'rdev': 0, 'dev': 0,
                         'mode': 0, '__pad1': 0, 'nlink': 0, 'ino': 0, 'blksize': 0, 'atime_nsec': 0,
                         'mtime': 0, 'ctime_nsec': 0, 'gid': 0, 'atime': 0, '__unused5': 0, '__unused4': 0, 'ctime': 0},
                'child_number': 0, 'result': -1}
################## test config ##################
# TODO empty meta file content
# TODO random meta file content


class NormalMetaPathCase(Case):
    """
    test_hcfs_parse_meta_NormalMetaPath:
          1.Call API with normal meta file path
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result stat must equal with expected stat
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup parse_meta spec")
        self.parse_meta_spec = FuncSpec([str], [self.get_output_spec_str()])

    def test(self):
        for expected_stat, meta_path in self.get_stat_and_meta_path_pairs():
            result = parse_meta(meta_path)
            # RD said there aren't some fields stored in the meta file
            self.ignore_fields(expected_stat, result)
            isPass, msg = self.parse_meta_spec.check_onNormal(
                [meta_path], [result])
            if not isPass:
                return False, msg
            if result != expected_stat:
                return False, "Result doesn't match with expected" + repr((meta_path, result, expected_stat))
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def get_output_spec_str(self):
        output_spec_str = {"file_type": int,
                           "child_number": int, "result": int}
        stat_spec_str = {"blocks": int, "uid": int, "__unused5": int, "mtime_nsec": int, "rdev": int,
                         "dev": int, "ctime": int, "__pad1": int, "blksize": int, "nlink": int,
                         "mode": int, "atime_nsec": int, "mtime": int, "ctime_nsec": int, "gid": int,
                         "atime": int, "ino": int, "__unused4": int, "size": int}
        output_spec_str["stat"] = stat_spec_str
        return output_spec_str

    def get_stat_and_meta_path_pairs(self):
        # test_data
        #  |  12/meta_12   (meta)
        #  |  12/12             (stat)
        for dir_path, dir_names, _ in os.walk(TEST_DATA_DIR):
            for name in (x for x in dir_names if x.isdigit()):
                stat = self.get_stat(os.path.join(dir_path, name, name))
                meta_path = os.path.join(dir_path, name, "meta_" + name)
                yield (stat, os.path.abspath(meta_path))

    def get_stat(self, path):
        with open(path, "rt") as fin:
            return ast.literal_eval(fin.read())

    def ignore_fields(self, expected, result):
        # Disabled atime, rdev, dev, uid, gid, mode
        # atime change when stat the expected value
        # rdev and dev hcfs don't store these value
        # uid, gid and mode ???
        expected["stat"]["atime"] = result["stat"]["atime"]
        expected["stat"]["rdev"] = result["stat"]["rdev"]
        expected["stat"]["dev"] = result["stat"]["dev"]
        expected["stat"]["uid"] = result["stat"]["uid"]
        expected["stat"]["gid"] = result["stat"]["gid"]
        expected["stat"]["mode"] = result["stat"]["mode"]  # ???


# inheritance NormalMetaPathCase(setUp, tearDown)
class NonexistMetaPathCase(NormalMetaPathCase):
    """
    test_hcfs_parse_meta_NonexistMetaPath:
          1.Call API with non-existed meta file path
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result must be the same as definition error return
    """

    def test(self):
        nonexisted_path = ["/no", "/such/", "/no/such/meta", "/and/directory"]
        for path in nonexisted_path:
            result = parse_meta(path)
            isPass, msg = self.parse_meta_spec.check_onNormal([path], [result])
            if not isPass:
                return False, msg
            if result != EXPECTED_ERR:
                return False, "Result doesn't match with expected err" + repr((result, EXPECTED_ERR))
        return True, ""


# inheritance NormalMetaPathCase(setUp, tearDown)
class EmptyMetaPathCase(NormalMetaPathCase):
    """
    test_hcfs_parse_meta_EmptypathMetapath:
        1.Call API with empty meta file path
        2.(Expected) Result matched with API input and normal output spec
        3.(Expected) Result must be the same as definition error return
    """

    def test(self):
        result = parse_meta("")
        isPass, msg = self.parse_meta_spec.check_onNormal([""], [result])
        if not isPass:
            return False, msg
        if result != EXPECTED_ERR:
            return False, "Result doesn't match with expected err" + repr((result, EXPECTED_ERR))
        return True, ""

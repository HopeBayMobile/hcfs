import os

from Case import Case
import config
from Utils.metaParserAdapter import *
from Utils.FuncSpec import FuncSpec
from Utils.tedUtils import listdir_full, listdir_path, negate
from constant import Path


class NormalCase(Case):
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
        self.set_spec()
        instances = self.func_spec.gen_zero_instance_by_output_spec()
        self.ERR_RESULT = instances[0]
        self.ERR_RESULT["result"] = -1

    def test(self):
        for expected_stat, path in self.get_stat_and_meta_path_pairs():
            result = parse_meta(path)
            # RD said there aren't some fields stored in the meta file
            self.ignore_fields(expected_stat, result)
            isPass, msg = self.func_spec.check_onNormal([path], [result])
            if not isPass:
                return False, msg
            if result != expected_stat:
                return False, "Result doesn't match with expected" + repr((meta_path, result, expected_stat))
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def set_spec(self):
        # parse_meta(b"test_data/v1/android/meta_isdir")
        # { 'child_number': 1000, 'file_type': 0,'result': 0,
          # 'stat': { '__pad1': 0, 'atime': 1470911895, 'atime_nsec': 0,
          #           'blksize': 1048576, 'blocks': 0,
          #           'ctime': 1470911938, 'ctime_nsec': 0, 'dev': 0,
          #           'gid': 0, 'ino': 6088, 'magic': [104, 99, 102, 115],
          #           'metaver': 1, 'mode': 16877,
          #           'mtime': 1470911938, 'mtime_nsec': 0, 'nlink': 1002,
          #           'rdev': 0, 'size': 0, 'uid': 0}}
          # { 'child_number': 0, 'error_msg': 'parse_meta: Unsupported meta version', 'file_type': 0, 'result': -2, 'stat' : {all zer0}}
          # stderr : Error: parse_meta: Unsupported meta version
        output_spec_str = {"file_type": int,
                           "child_number": int, "result": int}
        stat_spec_str = {"__pad1": int, "atime": int, "atime_nsec": int,
                         "blksize": int, "blocks": int,
                         "ctime": int, "ctime_nsec": int, "dev": int,
                         "gid": int, "ino": int, "magic": [int, int, int, int],
                         "metaver": int, "mode": int,
                         "mtime": int, "mtime_nsec": int, "nlink": int,
                         "rdev": int, "size": int, "uid": int}
        output_spec_str["stat"] = stat_spec_str
        self.func_spec = FuncSpec([str], [output_spec_str])

    def get_stat_and_meta_path_pairs(self):
        for path, ino in listdir_full(Path.TEST_DATA_DIR, str.isdigit):
            stat = self.get_stat(path, ino)
            meta_path = os.path.join(path, "meta_" + ino)
            yield (stat, meta_path)

    def get_stat(self, path, ino):
        stat_path = os.path.join(path, ino)
        with open(stat_path, "rt") as fin:
            return ast.literal_eval(fin.read())

    def ignore_fields(self, expected, result):
        # HCFS additional fields
        # 'magic' used as meta version check
        expected["stat"]["magic"] = result["stat"]["magic"]
        expected["stat"]["metaver"] = result["stat"]["metaver"]
        # change when access, disabled it
        expected["stat"]["atime"] = result["stat"]["atime"]
        # ignore human readable error message
        if "error_msg" in result:
            expected["error_msg"] = result["error_msg"]
        # Disabled rdev and dev which hcfs don't store these value
        expected["stat"]["rdev"] = result["stat"]["rdev"]
        expected["stat"]["dev"] = result["stat"]["dev"]
        # hcfs dynamic give these value, so these value in meta file are
        # non-sense
        if expected["location"] == "sdcard":
            expected["stat"]["mode"] = result["stat"]["mode"]
            expected["stat"]["uid"] = result["stat"]["uid"]
            expected["stat"]["gid"] = result["stat"]["gid"]
        expected.pop("location", None)


# inheritance NormalMetaPathCase(setUp, tearDown)
class RandomFileContentCase(NormalCase):
    """
    test_hcfs_parse_meta_random_content_file:
          1.Call API with random content file path(fsmgr, FSstat, data block, empty, random content)
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result code must be less than 0
    """

    def test(self):
        notstartswith = negate(str.startswith)
        for path in listdir_path(Path.TEST_RANDOM_DIR, notstartswith, ("meta",)):
            result = parse_meta(path)
            isPass, msg = self.func_spec.check_onNormal([path], [result])
            if not isPass:
                return False, msg
            if result["result"] >= 0:
                return False, "Result code must be less than 0:" + repr(result)
        return True, ""


# inheritance NormalMetaPathCase(setUp, tearDown)
class NonexistedAndEmptyPathCase(NormalCase):
    """
    test_hcfs_parse_meta_NonexistedAndEmptyPath:
          1.Call API with non-existed and empty file path
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result must be the same as definition error return
    """

    def test(self):
        nonexisted_path = ["/no/such/", "/no/such/meta", "/and/directory", ""]
        for path in nonexisted_path:
            result = parse_meta(path)
            isPass, msg = self.func_spec.check_onNormal([path], [result])
            if not isPass:
                return False, msg
            if "error_msg" in result:
                self.ERR_RESULT["error_msg"] = result["error_msg"]
            if result != self.ERR_RESULT:
                return False, "Result doesn't match with expected err" + repr((result, self.ERR_RESULT))
        return True, ""

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
# TODO empty meta file content
# TODO random meta file content


class NormalCase(Case):
    """
    test_hcfs_parse_meta_NormalMetaPath:
          1.Call API with normal meta file path
          2.(Expected) Result matched with API input and normal output spec
          3.(Expected) Result stat must equal with expected stat
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.log_file = LogFile(REPORT_DIR, "meta_parser")
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Setup parse_meta spec")
        self.set_spec()
        instances = self.parse_meta_spec.gen_zero_instance_by_output_spec()
        self.ERR_RESULT = instances[0]
        self.ERR_RESULT["result"] = -1

    def test(self):
        for expected_stat, meta_path in self.get_stat_and_meta_path_pairs():
            result = parse_meta(meta_path)
            self.log_file.recordFunc("parse_meta", meta_path, result)
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
        self.parse_meta_spec = FuncSpec([str], [output_spec_str])

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
        # HCFS additional fields
        expected["stat"]["magic"] = result["stat"]["magic"]  # TODO ???
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
            self.log_file.recordFunc("parse_meta", path, result)
            isPass, msg = self.parse_meta_spec.check_onNormal([path], [result])
            if not isPass:
                return False, msg
            if "error_msg" in result:
                self.ERR_RESULT["error_msg"] = result["error_msg"]
            if result != self.ERR_RESULT:
                return False, "Result doesn't match with expected err" + repr((result, self.ERR_RESULT))
        return True, ""

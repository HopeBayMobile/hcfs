import os

from Case import Case
import config
from Utils.metaParserAdapter import *
from Utils.log import LogFile

# Test config, do not change these value during program.
# These vars are final in term of Java.
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
TEST_DATA_DIR = os.path.join(THIS_DIR, "test_data_v2")
REPORT_DIR = os.path.join(THIS_DIR, "..", "report")
################## test config ##################


class ExternalVolHasFSstatCase(Case):
    """
    test_hcfs_behavior_external_vol_has_FSstat:
          1.Call API to parse fsmgr to get inode number
          2.(Expected) There should be file name "FSstat" with correct suffix inode number in swift server(in snapshot swift list)
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.log_file = LogFile(REPORT_DIR, "behavior")
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Do nothing")

    def test(self):
        fsmgr_path = os.path.join(TEST_DATA_DIR, "fsmgr")
        result = list_external_volume(fsmgr_path)
        self.log_file.recordFunc("list_external_volume", fsmgr_path, result)
        swift_list_path = os.path.join(TEST_DATA_DIR, "swift_list")
        with open(swift_list_path, "rt") as fin:
            fsstat_inode_list = [int(line.replace("FSstat", ""))
                                 for line in fin if line.startswith("FSstat")]
        for inode, name in result:
            if inode not in fsstat_inode_list:
                return False, "Missing FSstat file of (inode, name)" + repr((inode, name))
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")


# inheritance ExternalVolHasFSstatCase(setUp, tearDown)
class NonexistMetaPathCase(ExternalVolHasFSstatCase):
    """
    test_hcfs_behavior_list_file_blocks_exists:
          1.Call API to list file blocks
          2.(Expected) There should be file name "data" with correct suffix inode number in swift server(in snapshot swift list)
    """

    def test(self):
        for meta_path in self.get_file_meta_pathes():
            result = list_file_blocks(meta_path)
            self.log_file.recordFunc("list_file_blocks", meta_path, result)
            swift_list_path = os.path.join(TEST_DATA_DIR, "swift_list")
            with open(swift_list_path, "rt") as fin:
                data_list = [line.replace("\n", "")
                             for line in fin if line.startswith("data")]
            if result["result"] != 0:
                return False, "Fail to call list_file_blocks" + repr((meta_path, result))
            for data_name in result["block_list"]:
                if data_name not in data_list:
                    return False, "Data block not found in swift" + repr((data_name))
        return True, ""

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


# inheritance ExternalVolHasFSstatCase(setUp, tearDown)
class test_hcfs_behavior_FSstat_exists(ExternalVolHasFSstatCase):
    """
    test_hcfs_parse_meta_EmptypathMetapath:
        (Expected) There should be file name "FSstat" with correct suffix inode number in swift server(in snapshot swift list)
    """

    def test(self):
        swift_list_path = os.path.join(TEST_DATA_DIR, "swift_list")
        with open(swift_list_path, "rt") as fin:
            fsstat_inode_list = [int(line.replace("FSstat", ""))
                                 for line in fin if line.startswith("FSstat")]
        if self.get_data_data_inode() not in fsstat_inode_list:
            return False, "Missing /data/data FSstat file in swift"
        if self.get_data_app_inode() not in fsstat_inode_list:
            return False, "Missing /data/app FSstat file in swift"
        return True, ""

    def get_data_data_inode(self):
        path = os.path.join(TEST_DATA_DIR, "data_stat")
        with open(path, "rt") as fin:
            return ast.literal_eval(fin.read())["stat"]["ino"]

    def get_data_app_inode(self):
        path = os.path.join(TEST_DATA_DIR, "app_stat")
        with open(path, "rt") as fin:
            return ast.literal_eval(fin.read())["stat"]["ino"]

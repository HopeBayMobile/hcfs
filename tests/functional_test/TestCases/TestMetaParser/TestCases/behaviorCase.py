import os
import ast

from Case import Case
import config
from Utils.metaParserAdapter import pyhcfs
from Utils.tedUtils import listdir_full
from constant import FileType, Path


class ExternalVolHasFSstatCase(Case):
    """
    test_hcfs_behavior_external_vol_has_FSstat:
          1.Call API to parse fsmgr to get inode number
          2.(Expected) There should be file name "FSstat" with correct suffix inode number in swift server(in snapshot swift list)
    """

    def setUp(self):
        self.logger = config.get_logger().getChild(self.__class__.__name__)
        self.logger.info(self.__class__.__name__)
        self.logger.info("Setup")
        self.logger.info("Do nothing")

    def test(self):
        fsmgr_path = os.path.join(Path.TEST_DATA_DIR, "fsmgr")
        result = pyhcfs.list_external_volume(fsmgr_path)
        for inode, name in result:
            if inode not in self.get_swift_fsstat_inodes():
                return False, "Missing FSstat file of (inode, name)" + repr((inode, name))
        return True, ""

    def tearDown(self):
        self.logger.info("Teardown")
        self.logger.info("Do nothing")

    def get_swift_fsstat_inodes(self):
        swift_list_path = os.path.join(Path.TEST_DATA_DIR, "swift_list")
        with open(swift_list_path, "rt") as swift_list:
            fsstat_files = [x for x in swift_list if x.startswith("FSstat")]
            return [int(x.replace("\n", "").replace("FSstat", "")) for x in fsstat_files]


# inheritance ExternalVolHasFSstatCase(setUp, tearDown)
class NonexistMetaPathCase(ExternalVolHasFSstatCase):
    """
    test_hcfs_behavior_list_file_blocks_exists:
          1.Call API to list file blocks
          2.(Expected) There should be file name "data" with correct suffix inode number in swift server(in snapshot swift list)
    """

    def test(self):
        swift_data_list = self.get_swift_data_list()
        for meta_path in self.get_file_meta_pathes():
            result = pyhcfs.list_file_blocks(meta_path)
            if result["result"] != 0:
                return False, "Fail to call list_file_blocks" + repr((meta_path, result))
            for data_name in result["block_list"]:
                if data_name not in swift_data_list:
                    return False, "Data block not found in swift" + repr((data_name))
        return True, ""

    def get_swift_data_list(self):
        swift_list_path = os.path.join(Path.TEST_DATA_DIR, "swift_list")
        with open(swift_list_path, "rt") as swift_list:
            return [x.replace("\n", "") for x in swift_list if x.startswith("data")]

    def get_file_meta_pathes(self):
        for path, ino in listdir_full(Path.TEST_DATA_DIR, str.isdigit):
            if self.get_file_type(path, ino) == FileType.FILE:
                yield os.path.join(path, "meta_" + ino)

    def get_file_type(self, path, ino):
        stat_path = os.path.join(path, ino)
        with open(stat_path, "rt") as file:
            stat = ast.literal_eval(file.read())
            return stat["file_type"]


# inheritance ExternalVolHasFSstatCase(setUp, tearDown)
class test_hcfs_behavior_FSstat_exists(ExternalVolHasFSstatCase):
    """
    test_hcfs_parse_meta_EmptypathMetapath:
        (Expected) There should be file name "FSstat" with correct suffix inode number in swift server(in snapshot swift list)
    """

    def test(self):
        data_FSstat, app_FSstat = self.get_data_and_app_FSstat_name()
        swift_file_list = self.get_swift_file_list()
        if data_FSstat not in swift_file_list:
            return False, "Missing /data/data FSstat file in swift"
        if app_FSstat not in swift_file_list:
            return False, "Missing /data/app FSstat file in swift"
        return True, ""

    def get_swift_file_list(self):
        swift_list_path = os.path.join(Path.TEST_DATA_DIR, "swift_list")
        with open(swift_list_path, "rt") as swift_list:
            return [x.replace("\n", "") for x in swift_list]

    def get_data_and_app_FSstat_name(self):
        with open(os.path.join(Path.TEST_DATA_DIR, "data_stat"), "rt") as data_stat_file:
            with open(os.path.join(Path.TEST_DATA_DIR, "app_stat"), "rt") as app_stat_file:
                data_stat = ast.literal_eval(data_stat_file.read())
                app_stat = ast.literal_eval(app_stat_file.read())
                data_ino = str(data_stat["stat"]["ino"])
                app_ino = str(app_stat["stat"]["ino"])
                return "FSstat" + data_ino, "FSstat" + app_ino

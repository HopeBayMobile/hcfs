import os
import subprocess
from subprocess import Popen, PIPE
import time
import shutil
import string
import random

import config
from swift import Swift
import LogcatUtils
from socket import socketToMgmtApp
import HCFSConf
import adb


class TestMetaDataGenerator(object):

    def __init__(self, phone_id, fsmgr, test_data_dir, inodes, swift):
        self.logger = config.get_logger().getChild(__name__)
        self.phone_id = phone_id
        self.fsmgr = fsmgr
        self.test_data_dir = test_data_dir
        self.inodes = inodes
        self.swift = swift

    def get_data(self):
        if not self.isAvailable():
            return False
        self.get_swift_list()
        self.get_partition_stat()
        self.get_fsstat()
        self.get_random_data()
        self.get_fsmgr()
        self.get_test_data_stat()
        self.get_test_data_meta()
        return True

    def isAvailable(self):
        # TODO: swift server binds account, maybe we can change checking rule...
        # TODO: adb in docker, need 1.debug bridge, 2.devices
        # access to usb
        self.logger.info("Check if the USB-connected phone is Ted phone")
        return False if not adb.isAvailable(self.phone_id) else True

    def get_swift_list(self):
        self.logger.info("Get swift list")
        path = os.path.join(self.test_data_dir, "swift_list")
        out, err = self.swift.download_file(None, path)

    def get_partition_stat(self):
        self.logger.info("Get /sdcard stat")
        # TODO multiple external volume???
        fsmgr_stat = os.path.join(self.test_data_dir, "fsmgr_stat")
        with open(fsmgr_stat, "wt") as fout:
            data = self.get_file_stat("/storage/emulated")
            self.logger.debug("fetch" + repr((data, fsmgr_stat)))
            fout.write(repr(data))
        self.logger.info("Get /data/data stat")
        data_stat = os.path.join(self.test_data_dir, "data_stat")
        with open(data_stat, "wt") as fout:
            data = self.get_file_stat("/data/data")
            self.logger.debug("fetch" + repr((data, data_stat)))
            fout.write(repr(data))
        self.logger.info("Get /data/app stat")
        app_stat = os.path.join(self.test_data_dir, "app_stat")
        with open(app_stat, "wt") as fout:
            data = self.get_file_stat("/data/app")
            self.logger.debug("fetch" + repr((data, app_stat)))
            fout.write(repr(data))

    def get_fsstat(self):
        self.logger.info("Get FSstat")
        path = os.path.join(self.test_data_dir, "swift_list")
        with open(path, "rt") as fin:
            for file in fin:
                if file.startswith("FSstat"):
                    file = file.replace("\n", "")
                    self.logger.info("<" + file + ">")
                    fsstat_path = os.path.join(self.test_data_dir, file)
                    self.swift.download_file(file, fsstat_path)

    def get_random_data(self):
        random_dir = os.path.join(self.test_data_dir, "random")
        if os.path.exists(random_dir):
            shutil.rmtree(random_dir)
        os.makedirs(random_dir)
        self.logger.info("Get empty content file")
        empty_file_path = os.path.join(random_dir, "empty")
        with open(empty_file_path, "wt") as fout:
            pass
        self.logger.info("Get random content file")
        random_file_path = os.path.join(random_dir, "random")
        with open(random_file_path, "wt") as fout:
            fout.write(self.get_random_string(30))
        self.logger.info("Get data block file")
        swift_list_path = os.path.join(self.test_data_dir, "swift_list")
        with open(swift_list_path, "rt") as fin:
            for file in fin:
                if file.startswith("data"):
                    file = file.replace("\n", "")
                    self.logger.info("<" + file + ">")
                    fsstat_path = os.path.join(random_dir, file)
                    self.swift.download_file(file, fsstat_path)
                    break

    def get_fsmgr(self):
        self.logger.info("Get fsmgr")
        src = "/data/hcfs/metastorage/fsmgr"
        adb.get_file("fsmgr", src, self.fsmgr, self.phone_id)

    def get_test_data_meta(self):
        self.logger.info("Get metas")
        for inode in [str(self.stat_inode(path)) for path in self.inodes]:
            meta_name = "meta_" + inode
            new_path = os.path.join(self.test_data_dir, inode, meta_name)
            out, err = self.swift.download_file(meta_name, new_path)

    def get_test_data_stat(self):
        self.logger.info("Get stats")
        for path in self.inodes:
            self.logger.info("Create directory to store test data")
            inode = self.stat_inode(path)
            new_dir = os.path.join(self.test_data_dir, str(inode))
            isExisted = os.path.exists(new_dir)
            self.logger.debug("fetch" + repr((isExisted, new_dir)))
            if isExisted:
                continue
            os.makedirs(new_dir)

            self.logger.info("Get stat test data")
            new_prop = os.path.join(new_dir, str(inode))
            with open(new_prop, "wt") as fout:
                data = self.get_file_stat(path)
                self.logger.debug("fetch" + repr((data, new_prop)))
                fout.write(repr(data))
            assert os.path.isfile(new_prop), "Fail to get stat"

    def get_file_stat(self, path):
        result = {}
        result["file_type"] = self.stat_file_type(path)
        if result["file_type"] == 0:  # directory
            result["child_number"] = self.stat_child(path)
        else:
            result["child_number"] = 0
        result["result"] = 0
        if path.startswith("/sdcard/") or path.startswith("/storage/emulated/"):
            result["location"] = "sdcard"
        else:
            result["location"] = "local"
        stat = {}
        stat["blocks"] = self.stat_blocks(path)
        stat["ctime"] = self.stat_ctime(path)
        stat["mtime_nsec"] = 0
        stat["rdev"] = 0  # TODO: how to get this value???
        stat["dev"] = self.stat_dev(path)  # TODO: how to get this value???
        stat["mode"] = self.stat_mode(path)
        stat["__pad1"] = 0
        stat["ctime_nsec"] = 0
        stat["nlink"] = self.stat_nlink(path)
        stat["gid"] = self.stat_gid(path)
        stat["ino"] = self.stat_inode(path)
        stat["blksize"] = self.stat_blksize(path)
        stat["atime_nsec"] = 0
        stat["mtime"] = self.stat_mtime(path)
        stat["uid"] = self.stat_uid(path)
        stat["atime"] = self.stat_atime(path)
        stat["size"] = self.stat_size(path)  # byte
        result["stat"] = stat
        return result

    def stat_file_type(self, path):
        file_type = self.stat("F", path)
        if file_type == "directory":
            return 0
        elif file_type == "regular file":
            return 1
        elif file_type == "socket":
            return 4
        # TODO: link, pipe
        return -1

    def stat_child(self, path):
        cmd = "find " + path + " -mindepth 1 -maxdepth 1 | wc -l"
        out, err = adb.exec_cmd(cmd, self.phone_id)
        assert not err, "Stat <" + path + "> error"
        assert out.rstrip().isdigit(), "Stat error child number is not a integer"
        return int(out.rstrip()) + 2  # . and ..

    def stat_inode(self, path): return int(self.stat("i", path))

    def stat_blocks(self, path): return int(self.stat("b", path))

    def stat_blksize(self, path): return int(self.stat("B", path))

    def stat_uid(self, path): return int(self.stat("u", path))

    def stat_gid(self, path): return int(self.stat("g", path))

    def stat_atime(self, path): return int(self.stat("X", path))

    def stat_mtime(self, path): return int(self.stat("Y", path))

    def stat_ctime(self, path): return int(self.stat("Z", path))

    def stat_nlink(self, path): return int(self.stat("h", path))

    def stat_dev(self, path): return int(self.stat("d", path)[:-1])

    def stat_mode(self, path): return int(self.stat("f", path), 16)

    def stat_size(self, path): return int(self.stat("s", path))

    def stat(self, opt, path):
        cmd = "stat -c%" + opt + " " + path
        out, err = adb.exec_cmd(cmd, self.phone_id)
        assert not err, "Stat <" + path + "> error"
        assert out, "Stat <" + path + "> inode is empty"
        return out.rstrip()

    def get_random_string(self, size=6, chars=string.ascii_uppercase + string.digits):
        return ''.join(random.choice(chars) for _ in range(size))

if __name__ == '__main__':
    socketToMgmtApp.setup()
    socketToMgmtApp.refresh_token()
    socketToMgmtApp.cleanup()
    time.sleep(5)

    HCFSConf.setup()

    logcat = LogcatUtils.create_logcat_obj("HopeBay")
    isFound, timestamp, log = logcat.find_until("setSwiftToken")
    assert isFound, "Timeout while finding setSwiftToken API log in logcat"
    # HCFSMgmtUtils(setSwiftToken): what we need
    content = log.split(":", 1)[1].strip()
    # url=https://61.219.202.83:8080/swift/v1, token=XXXX,
    # result={"result":true,"code":0,"data":{}}
    url = content.split(", ")[0].split("=")[1]
    token = content.split(", ")[1].split("=")[1]
    json_result = content.split(", ")[2].split("=")[1]

    swift = Swift.via_token(url, HCFSConf.get_container(), token)
    HCFSConf.cleanup()

    this_dir = os.path.abspath(os.path.dirname(__file__))
    test_data_dir = os.path.join(this_dir, "..", "TestCases", "test_data_v2")
    phone_id = "00f28ec4cb50a4f2"
    fsmgr = os.path.join(test_data_dir, "fsmgr")
    inodes = ["/sdcard/DCIM/", "/sdcard/Download/1", "/data/data/2",
              "/data/data/com.hopebaytech.hcfsmgmt/hcfsapid_sock", "/data/data/com.hopebaytech.hcfsmgmt/databases/"]
    param = (phone_id, fsmgr, test_data_dir, inodes, swift)

    testDataGener = TestMetaDataGenerator(*param)
    testDataGener.get_data()

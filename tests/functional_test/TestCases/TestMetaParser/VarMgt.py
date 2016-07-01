import os
import SwiftMgt
import logging

from Data import DataSrcFactory

_log_level = logging.WARNING

# current path repo/tests/functional_test/TestCases/TestMetaParser
_repo = os.getcwd()
while not os.path.exists(_repo + "/.git"):
    _repo = os.path.abspath(os.path.join(_repo, os.pardir))

_hcfs = _repo + "/src/HCFS"
_cli = _repo + "/src/CLI_utils"
_API = _repo + "/src/API"

_swift_data = "/tmp/swift_data"
_meta = "/tmp/meta"
_block = "/tmp/block"
_mnt = "/tmp/mnt"

_test_data_dir = _repo + "/tests/functional_test/TestCases/TestMetaParser/test_data"

_test_fsmgr = _test_data_dir + "/fsmgr"
_phone_id = "00f28ec4cb50a4f2"
_phone_sync_inodes = ["/sdcard/DCIM/Camera", "/sdcard/DCIM",
                      "/sdcard/DCIM/Camera/IMG_20160623_143220.jpg"]
_swift = SwiftMgt.Swift("005FR0018SAL", "61.219.202.83", "WligAP3pSFWS",
                        "Z4O7HP3LFPPRKRJVGV99ZRUIYBDJSWCGQ2TAS80HB1I0PC76VJ1A1CLRAROHIO86TX8UC2QL5ZENAHO0687JT5D2JA924K3BOKY7RM6YRDBBVIKOM5XWP9HJ51QWSI37")

_hcfs_conf_tmp = _repo + "/tests/functional_test/TestCases/TestMetaParser/hcfs.conf.tmp"


def get_log_level():
    return _log_level


def get_repo():
    return _repo


def get_hcfs_path():
    return _hcfs


def get_cli_path():
    return _cli


def get_API_path():
    return _API


def get_swift_data_path():
    return _swift_data


def get_meta_path():
    return _meta


def get_block_path():
    return _block


def get_hcfs_conf_tmp():
    return _hcfs_conf_tmp


def get_mnt():
    return _mnt


def get_test_data_dir():
    return _test_data_dir


def get_all_meta_path():
    file_names = os.listdir(_test_data_dir)
    all_meta_path = []
    for name in file_names:
        dir_path = os.path.join(_test_data_dir, name)
        if os.path.isdir(dir_path) and name.isdigit():
            meta_path = os.path.join(dir_path, "meta_" + name)
            all_meta_path.extend([meta_path])
    return all_meta_path


def get_all_dir_meta_path():
    file_names = os.listdir(_test_data_dir)
    all_meta_path = []
    for name in file_names:
        dir_path = os.path.join(_test_data_dir, name)
        if os.path.isdir(dir_path) and name.isdigit():
            meta_path = os.path.join(dir_path, "meta_" + name)
            stat_path = os.path.join(dir_path, name)
            stat_src = DataSrcFactory.create_stat_src(stat_path)
            result, stat = stat_src.get_data()
            if result and stat["file_type"] == 0:
                all_meta_path.extend([meta_path])
    return all_meta_path


def get_all_data_path():
    file_names = os.listdir(_test_data_dir)
    all_data_path = []
    for name in file_names:
        dir_path = os.path.join(_test_data_dir, name)
        if os.path.isdir(dir_path) and name.isdigit():
            stat_path = os.path.join(dir_path, name)
            meta_path = os.path.join(dir_path, "meta_" + name)
            all_data_path.extend([(stat_path, meta_path)])
    return all_data_path


def get_meta_path(file_name):
    return os.path.join(_test_data_dir, file_name, "meta_" + file_name)


def get_test_fsmgr():
    return _test_fsmgr


def get_phone_id():
    return _phone_id


def get_phone_sync_inodes():
    return _phone_sync_inodes


def get_swift():
    return _swift

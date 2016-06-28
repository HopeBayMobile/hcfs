import os
import SwiftMgt

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
_phone_sync_dirs = "/sdcard/DCIM/Camera"
_swift = SwiftMgt.Swift("005FR0018SAL", "61.219.202.83", "WligAP3pSFWS", "Z4O7HP3LFPPRKRJVGV99ZRUIYBDJSWCGQ2TAS80HB1I0PC76VJ1A1CLRAROHIO86TX8UC2QL5ZENAHO0687JT5D2JA924K3BOKY7RM6YRDBBVIKOM5XWP9HJ51QWSI37")

_hcfs_conf_tmp = _repo + "/tests/functional_test/TestCases/TestMetaParser/hcfs.conf.tmp"

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

def get_test_fsmgr():
	return _test_fsmgr

def get_phone_id():
	return _phone_id

def get_phone_sync_dirs():
	return _phone_sync_dirs

def get_swift():
	return _swift

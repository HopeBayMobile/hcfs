import os

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

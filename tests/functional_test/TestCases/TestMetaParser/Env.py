import os
import subprocess
from subprocess import Popen, PIPE
import shutil
import logging
import time
import pwd
import grp
import getpass

import HCFSMgt
import SwiftMgt
import VarMgt

# TODO:path replace with os.path.join to feat all OS

_repo = VarMgt.get_repo()
_swift_data = VarMgt.get_swift_data_path()
_meta = VarMgt.get_meta_path()
_block = VarMgt.get_block_path()
_mnt = VarMgt.get_mnt()

_hcfs_conf_tmp = VarMgt.get_hcfs_conf_tmp()

logging.basicConfig()
_logger = logging.getLogger(__name__)
_logger.setLevel(logging.INFO)

# TODO: return permissioon? In the root
def prepare_dir_permission():
	_logger.info("[Env] Ggrant permission 730 to current user <" + getpass.getuser() + "> and change group to <fuse>")
	_logger.info("[Env] Affect below : /data /data/hcfs.conf.tmp /data/hcfs.conf repo")
	# /data
	if not os.path.exists("/data"):	subprocess.call("sudo mkdir /data", shell=True)	
	# /data/hcfs.conf.tmp
	if os.path.isfile("/data/hcfs.conf.tmp"):	subprocess.call("sudo rm /data/hcfs.conf.tmp", shell=True)
	# /data/hcfs.conf
	if os.path.isfile("/data/hcfs.conf"):	subprocess.call("sudo rm /data/hcfs.conf", shell=True)

# TODO:decouple with shell script
def before():
	_logger.info("[Env] Execute before script.")
	subprocess.call(" ".join(["sudo", _repo + "/utils/setup_dev_env.sh", "-m", "functional_test"]), shell=True)	
	subprocess.call(" ".join(["sudo", _repo + "/utils/setup_dev_env.sh", "-m", "docker_host"]), shell=True)	

def cleanup():
	_logger.info("[Env] Cleaning up")
	HCFSMgt.compile_hcfs()
	if HCFSMgt.is_hcfs_running():
		HCFSMgt.terminate_hcfs()
		count = 3
		while HCFSMgt.is_hcfs_running() and count >= 0:
			count = count - 1
			time.sleep(1)
		assert not HCFSMgt.is_hcfs_running(), "Unable to terminate hcfs daemon"
	HCFSMgt.clean_hcfs()
	_logger.info("[Env] Clean hcfs config files.")
	if os.path.isfile("/data/hcfs.conf.tmp"):	os.remove("/data/hcfs.conf.tmp")
	if os.path.isfile("/data/hcfs.conf"):	os.remove("/data/hcfs.conf")
	_logger.info("[Env] Clean test directories <repo/tmp>.")
	if os.path.exists(_repo + "/tmp"):	subprocess.call("sudo rm -rf " + _repo + "/tmp", shell=True)

def setup_test_dir():
	_logger.info("[Env] Setup meta and block directories.")
	os.makedirs(_meta)
	os.makedirs(_block)	
	os.makedirs(_mnt)	

# TODO: check /data is existed or not, always clean it before test?
def setup_hcfs_conf(swift):
	_logger.info("[Env] Setup hcfs config file.")
	assert os.path.isfile(_hcfs_conf_tmp), "hcfs config file template is missing in <" + _hcfs_conf_tmp + ">"
	with open("/data/hcfs.conf.tmp", "wt") as fout:
		with open(_hcfs_conf_tmp, "rt") as fin:
			for line in fin:
				fout.write(line.replace("{meta}", _meta).replace("{block}", _block).replace("{user}", swift._user).replace("{pwd}", swift._pwd).replace("{ip}", swift._ip).replace("{bucket}", swift._bucket))
	HCFSMgt.enc_hcfs_conf()

def setup_Ted_env():
	try:
		global _swift
		prepare_dir_permission()
		before()
		cleanup()
		_swift = SwiftMgt.Swift("tedchen", "10.10.99.120", "gkuVn4slZCJB", "tedchentestmeta")
		HCFSMgt.compile_hcfs()
		setup_test_dir()
		setup_hcfs_conf(_swift)
		HCFSMgt.start_hcfs()
		HCFSMgt.create_filesystem("test_fs")
		assert "test_fs" in HCFSMgt.list_filesystems(), "HCFS create file system failure"
		HCFSMgt.mount("test_fs", _mnt)
	except Exception as e:
		cleanup()
		return False, e
	return True, ""

def create_meta_parser_env():
	try:
		global _swift
		prepare_dir_permission()
		before()
		cleanup()
		_swift = SwiftMgt.Swift("tedchen", "10.10.99.120", "gkuVn4slZCJB", "tedchentestmeta")
		HCFSMgt.compile_hcfs()
		setup_test_dir()
		setup_hcfs_conf(_swift)
		HCFSMgt.start_hcfs()
		HCFSMgt.create_filesystem("test_fs")
		assert "test_fs" in HCFSMgt.list_filesystems(), "HCFS create file system failure"
		HCFSMgt.mount("test_fs", _mnt)
	except Exception as e:
		cleanup()
		return False, e
	return True, ""

# TODO: not yet implemented
def setup_docker_env():
	before()
	cleanup()
	_logger.info("[Env] Create swift data directories.")
	os.makedirs(_swift_data)

if __name__ == "__main__":
	setup_Ted_env()
	

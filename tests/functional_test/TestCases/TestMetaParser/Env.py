import os
import subprocess
from subprocess import Popen, PIPE
import shutil
import logging
import time
import pwd
import grp
import getpass

import HCFSmgt
import Var

# TODO:path replace with os.path.join to feat all OS

_repo = Var.get_repo()
_swift_data = Var.get_swift_data_path()
_meta = Var.get_meta_path()
_block = Var.get_block_path()
_mnt = Var.get_mnt()

_hcfs_conf_tmp = Var.get_hcfs_conf_tmp()

logging.basicConfig()
_logger = logging.getLogger(__name__)
_logger.setLevel(logging.INFO)

class _Swift(object):

	def __init__(self, user, ip, pwd, bucket):
		self._user = user
		self._ip = ip
		self._url = "https://" + ip + ":8080/auth/v1.0"
		self._pwd = pwd
		self._bucket = bucket
		self._useDocker = False
		_logger.info("[Swift] Initializing.")
		self.cleanup()
		self.create_bucket()
		self.check_bucket()

	@classmethod
	def fromDocker(cls, docker, user, pwd, bucket):
		swift = cls(user, docker.getIP(), pwd, bucket)
		swift._useDocker = True
		return swift

	def cleanup(self):
		_logger.info("[Swift] Cleaning up.")
		if not self._useDocker:
			self.rm_bucket()
		
	def rm_bucket(self):
		cmd = self._cmd_prefix()
		cmd.extend(["delete", self._bucket])
		_logger.info("[Swift] Remove bucket cmd = " + " ".join(cmd))	
		subprocess.call(" ".join(cmd), shell=True, stdout=PIPE, stderr=PIPE)

	def create_bucket(self):
		cmd = self._cmd_prefix()
		cmd.extend(["post", self._bucket])
		_logger.info("[Swift] Create bucket cmd = " + " ".join(cmd))
		subprocess.call(" ".join(cmd), shell=True, stdout=PIPE, stderr=PIPE)

	def check_bucket(self):
		cmd = self._cmd_prefix()
		cmd.extend(["list", self._bucket])
		_logger.info("[Swift] Check bucket cmd = " + " ".join(cmd))
		pipe = subprocess.Popen(" ".join(cmd), stdout=PIPE, stderr=PIPE, shell=True)
		out, err = pipe.communicate()
		assert not err, err
	
	def _cmd_prefix(self):
		return ["swift --insecure -A", self._url, "-U", self._user + ":" + self._user, "-K", self._pwd]

# TODO: not yet implemented
class _Docker(object):

	# name:str
	# volume:[(str,str,str), (str,str), (str) ...] =>(host src), dest, (options)
	# image:str
	def __init__(self, name, volume, image):
		self._name = name
		self._volume = [("/etc/localtime", "/etc/localtime", "ro")].extend(volume)
		self._image = image
		self._tty = True
		self._detach = True
		_logger.info("[Docker] Initializing.")
		self.cleanup()
		self.mk_HCFS_dirs()

	def run(self):
		cmds = ["sudo docker run"]

	def cleanup(self):
		_logger.info("[Docker] Cleaning up.")
		self.rm()

	def rm(self):
		cmd = ["sudo docker rm -f", self._name]
		_logger.info("[Docker] Stopping docker cmd = " + " ".join(cmd))
		subprocess.call(" ".join(cmd), shell=True)
	
	def getIP(self):
		cmds = ["", "delete", bucket]

# TODO: return permissioon? In the root
def prepare_dir_permission():
	_logger.info("[Env] Ggrant permission 730 to current user <" + getpass.getuser() + "> and change group to <fuse>")
	_logger.info("[Env] Affect below : /data /data/hcfs.conf.tmp /data/hcfs.conf repo")
	if not os.path.exists("/data"):	subprocess.call("sudo mkdir /data", shell=True)	
	subprocess.call("sudo chown " + getpass.getuser() + ":fuse /data", shell=True)
	subprocess.call("sudo chmod 730 /data", shell=True)
	if os.path.isfile("/data/hcfs.conf.tmp"):	subprocess.call("sudo rm /data/hcfs.conf.tmp", shell=True)
	if os.path.isfile("/data/hcfs.conf"):	subprocess.call("sudo rm /data/hcfs.conf", shell=True)
	subprocess.call("sudo chown " + getpass.getuser() + ":fuse " + _repo, shell=True)
	subprocess.call("sudo chmod 730 " + _repo, shell=True)

# TODO:decouple with shell script
def before():
	_logger.info("[Env] Execute before script.")
	subprocess.call(" ".join(["sudo", _repo + "/utils/setup_dev_env.sh", "-m", "functional_test"]), shell=True)	
	subprocess.call(" ".join(["sudo", _repo + "/utils/setup_dev_env.sh", "-m", "docker_host"]), shell=True)	
	subprocess.call(["sudo", "/bin/sh", "-c", _repo + "/utils/env_config.sh"])

def cleanup():
	_logger.info("[Env] Cleaning up")
	HCFSmgt.compile_hcfs()
	if HCFSmgt.is_hcfs_running():
		HCFSmgt.terminate_hcfs()
		count = 3
		while HCFSmgt.is_hcfs_running() and count >= 0:
			count = count - 1
			time.sleep(1)
		assert not HCFSmgt.is_hcfs_running(), "Unable to terminate hcfs daemon"
	_logger.info("[Env] Clean test data directories.")
	if os.path.exists(_repo + "/tmp"):	shutil.rmtree(_repo + "/tmp")
	_logger.info("[Env] Clean hcfs config files.")
	if os.path.isfile("/data/hcfs.conf.tmp"):	os.remove("/data/hcfs.conf.tmp")
	if os.path.isfile("/data/hcfs.conf"):	os.remove("/data/hcfs.conf")
	HCFSmgt.clean_hcfs()

# TODO: directory access need to check permission
def setup():
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
	HCFSmgt.enc_hcfs_conf()

def setup_Ted_env():
	try:
		global _swift
		prepare_dir_permission()
		before()
		cleanup()
		setup()
		_swift = _Swift("tedchen", "10.10.99.120", "gkuVn4slZCJB", "tedchentestmeta")
		HCFSmgt.compile_hcfs()
		setup_hcfs_conf(_swift)
		HCFSmgt.start_hcfs()
	except Exception as e:
		cleanup()
		return False, e
	return True, ""

# TODO: not yet implemented
def setup_docker_env():
	assert os.path.isfile("/var/run/docker.sock"), "Need installed docker."
	before()
	cleanup()
	setup()
	_logger.info("[Env] Create swift data directories.")
	os.makedirs(_swift_data)

if __name__ == "__main__":
	setup_Ted_env()
	

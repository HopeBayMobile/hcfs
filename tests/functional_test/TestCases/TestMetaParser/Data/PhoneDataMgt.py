import os
import subprocess
from subprocess import Popen, PIPE
import shutil
from overrides import overrides
import logging
import ast

from DataSrc import DataSrc

logging.basicConfig()
g_logger = logging.getLogger(__name__)
g_logger.setLevel(logging.INFO)

class PhoneDataSrc(DataSrc):
	def __init__(self, phone_id, fsmgr, test_data_dir, sync_dirs):
		self.phone_id = phone_id
		self.fsmgr = fsmgr
		self.test_data_dir = test_data_dir
		self.sync_dirs = sync_dirs
		self.new_dir = ""
		self.logger = g_logger.getChild("PhoneDataSrc")
		self.logger.setLevel(logging.INFO)

	@overrides
	def isAvailable(self):
		#TODO: swift server binds account, maybe we can change checking rule...
		#TODO: adb in docker, need 1.adb binary, 2.debug bridge, 3.devices access to usb
		pipe = subprocess.Popen("adb get-serialno | grep " + self.phone_id, shell=True, stdout=PIPE, stderr=PIPE)
		out, err = pipe.communicate()
		self.logger.info("Is usb connected and the phone is Ted phone. out = <" + out + ">, err = <" + err + ">")
		if not out:	return False

		pipe = subprocess.Popen("adb root | grep 'adbd is already running as root'", shell=True, stdout=PIPE, stderr=PIPE)
		out, err = pipe.communicate()
		self.logger.info("Is root. out = <" + out + ">, err = <" + err + ">")
		return True if out else False

	@overrides
	def fetch(self):
		try:
			self.logger.info("Get fsmgr")
			if os.path.isfile(self.fsmgr):	os.remove(self.fsmgr)
			subprocess.call("adb pull /data/hcfs/metastorage/fsmgr " + self.fsmgr, shell=True)
			
			#TODO: file type, file size
			self.logger.info("Create test file and push into phone device")
			test_file = "testFile.hp"
			test_file_path = os.path.join("/tmp", test_file)
			with open(test_file_path, "wt") as fout:	fout.write("test file content")
			subprocess.call("adb push " + test_file_path + " " + self.sync_dirs, shell=True)
			test_file_path = os.path.join(self.sync_dirs, test_file)

			self.logger.info("Create directory to store test data")
			inode = stat_inode(test_file_path)
			self.new_dir = os.path.join(self.test_data_dir, str(inode))
			if os.path.exists(self.new_dir):	shutil.rmtree(self.new_dir)
			os.makedirs(self.new_dir)

			self.logger.info("Get stat test data")
			new_prop = os.path.join(self.new_dir, str(inode))
			with open(new_prop, "wt") as fout:
				fout.write(repr(get_stat(test_file_path)))
			assert os.path.isfile(new_prop), "Stat <" + str(inode) + "> fail"
			with open(new_prop, "rt") as fin:
				return ast.literal_eval(fin.read())
		except Exception as e:
			if os.path.exists(self.new_dir):	shutil.rmtree(self.new_dir)
			raise e

def get_stat(path):
	result = {}
	result["file_type"] = stat_file_type(path)
	result["ino"] = stat_inode(path)
	result["blocks"] = stat_blocks(path)
	result["blksize"] = stat_blksize(path)
	result["uid"] = stat_uid(path)
	result["gid"] = stat_gid(path)
	result["atime"] = stat_atime(path)
	result["mtime"] = stat_mtime(path)
	result["ctime"] = stat_ctime(path)
	result["nlink"] = stat_nlink(path)
	return result

def stat_file_type(path):
	file_type = stat("F", path)
	if file_type == "directory":	return 0
	elif file_type == "regular file":	return 1
	#TODO: link, pipe, socket
	return -1

def stat_inode(path):	return long(stat("i", path))
def stat_blocks(path):	return int(stat("b", path))
def stat_blksize(path):	return int(stat("B", path))
def stat_uid(path):	return int(stat("u", path))
def stat_gid(path):	return int(stat("g", path))
def stat_atime(path):	return long(stat("X", path))
def stat_mtime(path):	return long(stat("Y", path))
def stat_ctime(path):	return long(stat("Z", path))
def stat_nlink(path):	return int(stat("h", path))

def stat(opt, path):
	pipe = subprocess.Popen("adb shell stat -c%" + opt + " " + path, shell=True, stdout=PIPE, stderr=PIPE)
	out, err = pipe.communicate()
	assert not err, "Stat <" + path + "> error = <" + err + ">"
	assert out, "Stat <" + path + "> inode is empty"
	return out.rstrip()

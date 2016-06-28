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
	def __init__(self, phone_id, fsmgr, test_data_dir, inodes):
		self.phone_id = phone_id
		self.fsmgr = fsmgr
		self.test_data_dir = test_data_dir
		self.inodes = inodes
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
		self.logger.info("Get fsmgr")
		if os.path.isfile(self.fsmgr):	os.remove(self.fsmgr)
		subprocess.call("adb pull /data/hcfs/metastorage/fsmgr " + self.fsmgr, shell=True)
			
		#TODO: file type, file size
		#self.logger.info("Create test file and push into phone device")
		#test_file = "testFile.hp"
		#test_file_path = os.path.join("/tmp", test_file)
		#with open(test_file_path, "wt") as fout:	fout.write("test file content")
		#subprocess.call("adb push " + test_file_path + " " + self.sync_dirs, shell=True)
		#test_file_path = os.path.join(self.sync_dirs, test_file)
			
		result = []
		for path in self.inodes:
			self.logger.info("Create directory to store test data")
			inode = stat_inode(path)
			new_dir = os.path.join(self.test_data_dir, str(inode))
			if os.path.exists(new_dir):	continue
			os.makedirs(new_dir)
				
			self.logger.info("Get stat test data")
			new_prop = os.path.join(new_dir, str(inode))
			with open(new_prop, "wt") as fout:
				fout.write(repr(get_stat(path)))
			assert os.path.isfile(new_prop), "Stat <" + str(inode) + "> fail"
			with open(new_prop, "rt") as fin:
				result.extend([ast.literal_eval(fin.read())])
		return result

def get_stat(path):
	result = {}
	stat = {}
	result["file_type"] = stat_file_type(path)
	result["child_number"] = stat_child(path) # not sure
	result["result"] = 0
	
	stat["blocks"] = stat_blocks(path)
	stat["uid"] = stat_uid(path)
	stat["__unused5"] = 0 #???
	stat["mtime_nsec"] = 0L
	stat["rdev"] = stat_rdev(path) 
	stat["dev"] = 0L #??? 
	stat["ctime"] = stat_ctime(path)
	stat["__pad1"] = 0 #??? about irq
	stat["blksize"] = stat_blksize(path)
	stat["nlink"] = stat_nlink(path)
	stat["mode"] = stat_mode(path) # not sure
	stat["atime_nsec"] = 0L
	stat["mtime"] = stat_mtime(path)
	stat["ctime_nsec"] = 0L
	stat["gid"] = stat_gid(path)
	stat["atime"] = stat_atime(path) # change when access, disabled it
	stat["ino"] = stat_inode(path)
	stat["__unused4"] = 0 #???
	stat["size"] = stat_size(path) # byte
	result["stat"] = stat
	return result

def stat_file_type(path):
	file_type = stat("F", path)
	if file_type == "directory":	return 0
	elif file_type == "regular file":	return 1
	#TODO: link, pipe, socket
	return -1

def stat_child(path):
	pipe = subprocess.Popen("adb shell find " + path + " -mindepth 1 | wc -l", shell=True, stdout=PIPE, stderr=PIPE)
	out, err = pipe.communicate()
	assert not err, "Stat <" + path + "> error = <" + err + ">"
	assert out.rstrip().isdigit(), "Stat <" + path + "> child number <" + out.rstrip() + "> is not a integer"
	return long(out.rstrip())

def stat_inode(path):	return long(stat("i", path))
def stat_blocks(path):	return int(stat("b", path))
def stat_blksize(path):	return int(stat("B", path))
def stat_uid(path):	return int(stat("u", path))
def stat_gid(path):	return int(stat("g", path))
def stat_atime(path):	return int(stat("X", path))
def stat_mtime(path):	return int(stat("Y", path))
def stat_ctime(path):	return int(stat("Z", path))
def stat_nlink(path):	return long(stat("h", path))
def stat_rdev(path):	return long(stat("d", path)[:-1])
def stat_mode(path):	return int(stat("f", path), 16)
def stat_size(path):	return int(stat("s", path)) 

def stat(opt, path):
	pipe = subprocess.Popen("adb shell stat -c%" + opt + " " + path, shell=True, stdout=PIPE, stderr=PIPE)
	out, err = pipe.communicate()
	assert not err, "Stat <" + path + "> error = <" + err + ">"
	assert out, "Stat <" + path + "> inode is empty"
	return out.rstrip()

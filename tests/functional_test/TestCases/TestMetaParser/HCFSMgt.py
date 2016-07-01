import logging
import subprocess
from subprocess import Popen, PIPE
import re
import os
import time

import VarMgt

logging.basicConfig()
_logger = logging.getLogger(__name__)
_logger.setLevel(logging.INFO)

_repo = VarMgt.get_repo()
_hcfs = VarMgt.get_hcfs_path()
_cli = VarMgt.get_cli_path()
_API = VarMgt.get_API_path()
_hcfs_bin = _hcfs + "/hcfs"
_HCFSvol_bin = _cli + "/HCFSvol"
_hcfsconf_bin = _API + "/hcfsconf"

# default timeout is 40 second
_hcfs_timeout = 40

def clean_hcfs():
	_logger.info("[HCFS] Make clean hcfs.")
	subprocess.call("sudo make -s -C " + _hcfs + " clean", shell=True, stdout=PIPE, stderr=PIPE)
	subprocess.call("sudo make -s -C " + _cli + " clean", shell=True, stdout=PIPE, stderr=PIPE)
	subprocess.call("sudo make -s -C " + _API + " clean", shell=True, stdout=PIPE, stderr=PIPE)
	assert not os.path.isfile(_hcfs_bin), "Error occured when make clean hcfs."
	assert not os.path.isfile(_HCFSvol_bin), "Error occured when make clean HCFSvol."
	assert not os.path.isfile(_hcfsconf_bin), "Error occured when make clean hcfsconf."

# TODO: "make" args is not for sure.
def compile_hcfs():
	_logger.info("[HCFS] Compile hcfs.")
	subprocess.call("sudo make -s -C " + _hcfs, shell=True, stdout=PIPE, stderr=PIPE)
	subprocess.call("sudo make -s -C " + _cli, shell=True, stdout=PIPE, stderr=PIPE)
	subprocess.call("sudo make -s -C " + _API + " hcfsconf", shell=True, stdout=PIPE, stderr=PIPE)
	assert os.path.isfile(_hcfs_bin), "Cannot make hcfs executable."
	assert os.access(_hcfs_bin, os.X_OK), "Cannot execute hcfs executable."
	assert os.path.isfile(_HCFSvol_bin), "Cannot make HCFSvol executable."
	assert os.access(_HCFSvol_bin, os.X_OK), "Cannot execute HCFSvol executable."
	assert os.path.isfile(_hcfsconf_bin), "Cannot make hcfsconf executable."
	assert os.access(_hcfsconf_bin, os.X_OK), "Cannot execute hcfsconf executable."

def enc_hcfs_conf():
	_logger.info("[HCFS] Encrypt config file.")
	pipe = subprocess.Popen(_hcfsconf_bin + " enc /data/hcfs.conf.tmp /data/hcfs.conf", stdout=PIPE, stderr=PIPE, shell=True)
	out, err = pipe.communicate()
	assert not err and not out, (out, err)
	assert os.path.isfile("/data/hcfs.conf"), "Error occured when encrypt hcfs config file."

def start_hcfs():
	_logger.info("[HCFS] Start hcfs.")
	# async subprocess hcfs daemon
	subprocess.Popen(_hcfs_bin + " -d -oallow_other", shell=True)
	count = 3
	while not is_hcfs_running() and count >= 0:
		count = count - 1
		time.sleep(1)
	assert is_hcfs_running(), "Unable to start hcfs daemon"

def is_hcfs_running(timeout=None):
	_logger.info("[HCFS] Check the process is running, wait for timeout at most 40 (default).")
	timeout = _hcfs_timeout
	while (timeout > 0):
		pipe = subprocess.Popen(["ps", "aux"], stdout=PIPE, stderr=PIPE)
		out, err = pipe.communicate()
		isRunning = False
		for line in out.split('\n'):
			if re.search("/hcfs( |$)", line):
				isRunning = True
		time.sleep(1)
		if isRunning:
			timeout = timeout - 1
		else:
			return False
	return True

def terminate_hcfs():
	_logger.info("[HCFS] Terminate the HCFS process.")
	pipe = subprocess.Popen([_HCFSvol_bin, "terminate"], stdout=PIPE, stderr=PIPE)
	out, err = pipe.communicate()
	assert not err, err
	_logger.info("[HCFS] " + out)

def list_filesystems():
	_logger.info("[HCFS] List file system.")
	pipe = subprocess.Popen([_HCFSvol_bin, "list"], stdout=PIPE, stderr=PIPE)
	out, err = pipe.communicate()
	assert not err, err
	return out

def create_filesystem(fs, isInternal=True):
	_logger.info("[HCFS] Create file system.")
	pipe = subprocess.Popen([_HCFSvol_bin, "create", fs, "internal" if isInternal else "external"], stdout=PIPE, stderr=PIPE)
	out, err = pipe.communicate()
	assert not err, err
	return out

def mount(fs, mount_point):
	_logger.info("[HCFS] Mount hcfs to <" + mount_point + "> with file system <" + fs + ">")
	assert os.path.exists(mount_point), "No such directory to mount hcfs."
	pipe = subprocess.Popen([_HCFSvol_bin, "mount", fs, mount_point], stdout=PIPE, stderr=PIPE)
	out, err = pipe.communicate()
	assert not err, err
	return out

def restart_hcfs():
	_logger.info("[HCFS] Restart hcfs.")
	terminate_hcfs()
	start_hcfs()

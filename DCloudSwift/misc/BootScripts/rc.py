import os
import subprocess
import logging
import sys
import time
import json
import pickle
import socket
import logging
import threading
import re

FORMATTER = '[%(levelname)s from %(name)s on %(asctime)s] %(message)s'
lockFile="/etc/delta/swift.lock"

class TryLockError(Exception):
	pass

# tryLock decorator
def tryLock(tries=1, lockTimeout=900):
	def deco_tryLock(fn):
		def wrapper(*args, **kwargs):
	
			returnVal = None
			locked = 1
			try:
				os.system("mkdir -p %s"%os.path.dirname(lockFile))
				cmd = "lockfile -11 -r %d -l %d %s"%(tries, lockTimeout, lockFile)
				locked = os.system(cmd)
				if locked == 0:
					returnVal = fn(*args, **kwargs) # first attempt
				else:
					raise TryLockError()
				return returnVal
			finally:
				if locked == 0:
					os.system("rm -f %s"%lockFile)

  		return wrapper #decorated function
  	
  	return deco_tryLock  # @retry(arg[, ...]) -> true decorator

def __loadSwiftMetadata(disk):
        logger = getLogger(name="__loadSwiftMetadata")
        mountpoint = "/tmp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


	#TODO: chechsum
	if mountDisk(disk, mountpoint) !=0:
                logger.error("Failed to mount %s"%disk)
                return 1

	returncode = 0

	cmd = "cp %s/swift/* /etc/swift/"%(mountpoint)
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
                logger.error("Failed to reload swift metadata from %s for %s"%(disk,output))
		returncode = 1

	if lazyUmount(mountpoint)!=0:
		logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))

	return returncode
def __loadScripts(disk):
        logger = getLogger(name="__loadSripts")
        mountpoint =  "/tmp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


        #TODO: chechsum
        if mountDisk(disk, mountpoint) !=0:
                logger.error("Failed to mount %s"%disk)
                return 1

        returncode = 0

        cmd = "cp -r %s/DCloudSwift /"%(mountpoint)
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()

        po.wait()
        if po.returncode != 0:
                logger.error("Failed to reload scripts from %s for %s"%(disk,output))
                returncode = 1

        if lazyUmount(mountpoint)!=0:
                logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))

        return returncode

def loadScripts():
        logger = getLogger(name="loadSripts")
        logger.info("start")

        disks = getNonRootDisks()

        latestFingerprint = getLatestFingerprint()
        os.system("mkdir -p /etc/swift")
        returncode = 1

        for disk in disks:
                (ret, fingerprint) = readFingerprint(disk)
                if ret==0 and fingerprint["vers"] >= latestFingerprint["vers"]:
                        if __loadScripts(disk) ==0:
                                returncode = 0
                                break

        logger.info("end")
        return returncode

def getAllDisks():
	cmd = "fdisk -l"
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()

	disks = []
	for line in lines:
		match = re.match(r"^Disk /dev/sd\w:", line)
		if match is not None:
			disks.append(line.split()[1][:8])

	return disks
	
def getRootDisk():
	cmd = "mount"
	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()

	device = lines[0].split()[0]
	return device[:8]



def getLatestFingerprint():
	logger = getLogger(name="getLatestFingerprint")
	logger.info("start")
	disks = getNonRootDisks()
	latest = None

       	for disk in disks:
		(ret, fingerprint) = readFingerprint(disk)
		if ret == 0:
			latest  = fingerprint  if latest is None or latest["vers"] < fingerprint["vers"]  else latest
			

	logger.info("end")
	return latest

def getNonRootDisks():
	rootDisk = getRootDisk()
	disks = getAllDisks()
	nonRootDisks =[]

	for disk in disks:
		if disk != rootDisk:
			nonRootDisks.append(disk)

	return nonRootDisks



def lazyUmount(mountpoint):
        logger = getLogger(name="umount")

        returncode = 0
        try:
		if os.path.ismount(mountpoint):
			cmd = "umount -l %s"%mountpoint
			po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
			output = po.stdout.read()
                 	po.wait()

                 	if po.returncode !=0:
                 		logger.error("Failed to umount -l %s for %s"%(mountpoint, output))
				returncode = 1

        except OSError as e:
                logger.error("Failed to umount -l %s for %s"%(disk, e))
		returncode = 1

        return returncode

def loadSwiftMetadata():
	logger = getLogger(name="loadSwiftMetadata")
	logger.info("start")

        disks = getNonRootDisks()
	latestFingerprint = getLatestFingerprint()
	os.system("mkdir -p /etc/swift")
	returncode = 1

        for disk in disks:
		(ret, fingerprint) = readFingerprint(disk)
		if ret==0 and fingerprint["vers"] >= latestFingerprint["vers"]:
			if __loadSwiftMetadata(disk) ==0:
				returncode = 0
				break

	os.system("chown -R swift:swift /etc/swift")

	logger.info("end")
	return returncode
	
def getLogger(name=None):
	"""
	Get a file logger using config settings.

	"""
	
	try:

		logger = logging.getLogger(name)

		if not hasattr(getLogger, 'handler4Logger'):
			getLogger.handler4Logger = {}

		if logger in getLogger.handler4Logger:
			return logger

	
		logDir = '/var/log/deltaSwift/'
		logName = 'rc.log'
		logLevel = 'INFO'

		os.system("mkdir -p "+logDir)
		os.system("touch "+logDir+'/'+logName)

		hdlr = logging.FileHandler(logDir+'/'+logName)
		hdlr.setFormatter(logging.Formatter(FORMATTER))
		logger.addHandler(hdlr)
		logger.setLevel(logLevel)
		logger.propagate = False	

		getLogger.handler4Logger[logger] = hdlr
		return logger
	finally:
		pass
	
def mountDisk(disk, mountpoint):
	logger = getLogger(name="mountDisk")

	returncode = 0
	try:
		os.system("mkdir -p %s"%mountpoint)
		if os.path.ismount(mountpoint):
			os.system("umount -l %s"%mountpoint)

		#TODO: Add timeout mechanism
                cmd = "mount %s %s"%(disk, mountpoint)
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
                po.wait()

                if po.returncode !=0:
                        logger.error("Failed to mount %s for %s"%(disk, output))
                        returncode =1

        except OSError as e:
                logger.error("Failed to mount %s for %s"%(disk, e))
                returncode = 1

	return returncode


def readFingerprint(disk):
        logger = getLogger(name="readFingerprint")
        mountpoint =  "/tmp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)
	fingerprint = {}

	#TODO: chechsum
	if mountDisk(disk, mountpoint) !=0:
                logger.debug("Failed to mount %s"%disk)
                return (1, fingerprint)

	try:
		with open("%s/fingerprint"%mountpoint, "rb") as fh:
			fingerprint = pickle.load(fh)

		return (0, fingerprint)
	except IOError as e:
		logger.debug("Failed to read fingerprint from %s for %s"%(disk, e))
		return (1, fingerprint)
	finally:
		if lazyUmount(mountpoint)!=0:
			logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))

@tryLock(1)
def main(argv):
    if not os.path.exists("/dev/shm/srv"):
        os.system("mkdir /dev/shm/srv")
        os.system("mount --bind /dev/shm/srv /srv")

    if not os.path.exists("/dev/shm/DCloudSwift"):
        os.system("mkdir /dev/shm/DCloudSwift")
        os.system("mount --bind /dev/shm/DCloudSwift /DCloudSwift")

    if os.path.exists("/tmp/i_am_not_zcw"):
        if loadScripts() == 0:
            os.system("python /DCloudSwift/maintenance.py -R")

if __name__ == '__main__':
	try:
		main(sys.argv[1:])
	except TryLockError:
		print >>sys.stderr, "A confilct task is in execution"
	except Exception as e:
		print >>sys.stderr, str(e)


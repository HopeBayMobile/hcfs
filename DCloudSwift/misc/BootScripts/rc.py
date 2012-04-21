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

logLock = threading.Lock()
FORMATTER = '[%(levelname)s from %(name)s on %(asctime)s] %(message)s'

def getLogger(name=None):
	"""
	Get a file logger using config settings.

	"""
	
	try:
		logLock.acquire()

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
		logLock.release()

def getRootDisk():
	cmd = "mount"
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	po.wait()

	firstLine = po.stdout.readline()
	device = firstLine.split()[0]
	return device[:8]

def getAllDisks():
	cmd = "fdisk -l|grep \"Disk /dev\" "
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	po.wait()

	disks = []
	for line in po.stdout.readlines():
		disks.append(line.split()[1][:8])

	return disks

def getNonRootDisks():
	rootDisk = getRootDisk()
	disks = getAllDisks()
	nonRootDisks =[]

	for disk in disks:
		if disk != rootDisk:
			nonRootDisks.append(disk)

	return nonRootDisks
		

def mountDisk(disk, mountpoint):
	logger = getLogger(name="mountDisk")

	returncode = 1
	try:
		os.system("mkdir -p %s"%mountpoint)
		if os.path.ismount(mountpoint):
                	os.system("umount -l %s"%mountpoint)

		#TODO: Add timeout mechanism
                cmd = "mount %s %s"%(disk, mountpoint)
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                po.wait()

                if po.returncode !=0:
                        logger.error("Failed to mount  %s for %s"%(disk, po.stderr.readline()))

		returncode = 0

        except OSError as e:
                logger.error("Failed to mount %s for %s"%(disk, e))

	return returncode

		
def readMetadata(disk):
        logger = getLogger(name="readMetadata")
	metadata = {}
        mountpoint =  "/temp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


	#TODO: chechsum
	if mountDisk(disk, mountpoint) !=0:
                logger.debug("Failed to mount %s"%disk)
                return (1, metadata)

	try:
		with open("%s/Metadata"%mountpoint, "rb") as fh:
			metadata = pickle.load(fh)

		return (0, metadata)
	except IOError as e:
		logger.debug("Failed to read metadata from %s for %s"%(disk, e))
		return (1, metadata)
	finally:
		if lazyUmount(mountpoint)!=0:
			logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))


def getLatestMetadata():
	logger = getLogger(name="getLatestMetadata")
	logger.info("start")
	disks = getNonRootDisks()
	latestMetadata = None

       	for disk in disks:
		(ret, metadata) = readMetadata(disk)
		if ret == 0:
			latestMetadata  = metadata  if latestMetadata is None or latestMetadata["vers"] < metadata["vers"]  else latestMetadata
			

	logger.info("end")
	return latestMetadata

def __loadScripts(disk):
        logger = getLogger(name="__loadSripts")
	metadata = {}
        mountpoint =  "/temp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


	#TODO: chechsum
	if mountDisk(disk, mountpoint) !=0:
                logger.error("Failed to mount %s"%disk)
                return 1

	returncode = 0

	cmd = "cp -r %s/DCloudSwift /"%(mountpoint)
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()
        if po.returncode != 0:
                logger.error("Failed to reload scripts from %s for %s"%(disk,po.stderr.readline()))
		returncode = 1

	if lazyUmount(mountpoint)!=0:
		logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))

	return returncode

def loadScripts():
	logger = getLogger(name="loadSripts")
	logger.info("start")

        disks = getNonRootDisks()
	latestMetadata = getLatestMetadata()
	os.system("mkdir -p /etc/swift")
	returncode = 1

        for disk in disks:
		(ret, metadata) = readMetadata(disk)
		if ret==0 and metadata["vers"] >= latestMetadata["vers"]:
			if __loadScripts(disk) ==0:
				returncode = 0
				break

	logger.info("end")
	return returncode


def lazyUmount(mountpoint):
        logger = getLogger(name="umount")

        returncode = 0
        try:
		if os.path.ismount(mountpoint):
                	cmd = "umount -l %s"%mountpoint
                	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                	po.wait()

                	if po.returncode !=0:
                        	logger.error("Failed to umount -l  %s for %s"%(mountpoint, po.stderr.readline()))
				returncode = 1

        except OSError as e:
                logger.error("Failed to umount -l %s for %s"%(disk, e))
		returncode = 1

        return returncode


def main(argv):
	if loadScripts() == 0:
		os.system("python /DCloudSwift/util/mountDisks.py -R")

if __name__ == '__main__':
	main(sys.argv[1:])
	#print loadScripts()
	#print getMajorityHostname()
	#print getLatestMetadata()
	#createSwiftDevices()
	#writeMetadata(disk="/dev/sdc", vers=1, deviceNum=4, devicePrx="sdb", deviceCnt=9)
	#writeMetadata(disk="/dev/sdd", vers=1, deviceNum=5, devicePrx="sdb", deviceCnt=9)
	#writeMetadata(disk="/dev/sde", vers=1, deviceNum=6, devicePrx="sdb", deviceCnt=9)
	#print getMajorityHostname()
	#print loadSwiftMetadata()
	#print readMetadata(disk="/dev/sdb")
	#print remountDisks()
	#print int(time.time())
	#resume()

import os
import subprocess
import logging
import threading
import sys
import time
import json
import pickle
import socket
import util
import re


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)

#TODO: Read from config files
UNNECESSARYFILES = "cert* backups *.conf"


class MountSwiftDeviceError(Exception): pass
class WriteMetadataError(Exception): pass

def getMajorityHostname():
	logger = util.getLogger(name="getMajorityHostname")
	logger.debug("start")
	disks = getNonRootDisks()
	hostnameCount = {}
	maxCount =0
	mojorityHostname=None

       	for disk in disks:
		(ret, metadata) = readMetadata(disk)
		if ret == 0:
			hostnameCount.setdefault(metadata["hostname"], 0)
			hostnameCount[metadata["hostname"]] +=1 
			
	for hostname in hostnameCount:
		if hostnameCount[hostname] > maxCount:
			maxCount = hostnameCount[hostname]
			majorityHostname = hostname

	latestMetadata = getLatestMetadata()

	if latestMetadata is not None:
		if (2*maxCount) <= latestMetadata["deviceCnt"]:
			majorityHostname = None		

	logger.debug("end")
	return majorityHostname

def getRootDisk():
	cmd = "mount"
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()
	

	device = lines[0].split()[0]
	return device[:8]

def getAllDisks():
	cmd = "fdisk -l"
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	lines = po.stdout.readlines()


	disks = []
	for line in lines:
		match = re.match(r"^Disk /dev/sd\w:", line)
		if match is not None: 
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
		

def formatDisks(diskList):
	logger = util.getLogger(name="formatDisks")
	returncode=0
	formattedDisks = []

	for disk in diskList:
                cmd = "mkfs.ext4 -F %s"%(disk)
                po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()

                if po.returncode != 0:
                        logger.error("Failed to format %s for %s"%(disk,output))
                        returncode+=1
                        continue

                formattedDisks.append(disk)


        return (returncode,formattedDisks)


def formatNonRootDisks(deviceCnt=1):
	'''
	Format deviceCnt non-root disks
	'''
	logger = util.getLogger(name="formatNonRootDisks")
	disks = getNonRootDisks()
	formattedDisks = []
	returncode=0

	for disk in disks:
		if len(formattedDisks) == deviceCnt:
			break

		cmd = "mkfs.ext4 -F %s"%(disk) 
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
		po.wait()

		if po.returncode != 0:
			logger.warn("Failed to format %s for %s"%(disk,output))
			continue

		formattedDisks.append(disk)

	returncode = 0 if deviceCnt - len(formattedDisks) ==0 else 0

	return (returncode,formattedDisks)


def mountDisk(disk, mountpoint):
	logger = util.getLogger(name="mountDisk")

	returncode = 0
	try:
		os.system("mkdir -p %s"%mountpoint)
		if os.path.ismount(mountpoint):
                	os.system("umount -l %s"%mountpoint)

		#TODO: Add timeout mechanism
                cmd = "mount -o user_xattr %s %s"%(disk, mountpoint)
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		output = po.stdout.read()
                po.wait()

                if po.returncode !=0:
                        logger.error("Failed to mount  %s for %s"%(disk, output))
                        returncode = 1

        except OSError as e:
                logger.error("Failed to mount %s for %s"%(disk, e))
                returncode = 1

	return returncode

		
def mountSwiftDevice(disk, devicePrx, deviceNum):
        logger = util.getLogger(name="mountSwiftDevice")

        mountpoint = "/srv/node/%s%d"%(devicePrx,deviceNum)
	returncode = mountDisk(disk, mountpoint)

        if returncode != 0:
        	logger.error("Failed to mount %s on %s"%(disk,mountpoint))

	return returncode

def createSwiftDevices(deviceCnt=3, devicePrx="sdb"):
        logger = util.getLogger(name="createSwiftDevices")
	logger.debug("start")

	lazyUmountSwiftDevices()
	(ret,disks)=formatNonRootDisks(deviceCnt)
	if ret != 0:
		return deviceCnt
	
	#TODO: modified to match the ring version
	vers = util.getSwiftConfVers() 
	count = 0
        for disk in disks:
                try:
                        count+=1
                        mountpoint = "/srv/node"+"/%s%d"%(devicePrx,count)
                        os.system("mkdir -p %s"%mountpoint)
                        if os.path.ismount(mountpoint):
                                os.system("umount -l %s"%mountpoint)

			print "%s\n"%mountpoint
                	#line = "%s %s xfs noatime,nodiratime,nobarrier,logbufs=8 0 0"%(disk, mountpoint)

                        if writeMetadata(disk=disk, vers=vers, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=count)!=0:
                                raise WriteMetadataError("Failed to write metadata into %s"%disk)

                        if mountSwiftDevice(disk=disk, devicePrx=devicePrx, deviceNum=count)!=0:
                                raise MountSwiftDeviceError("Failed to mount %s on %s"%(disk, mountpoint))

                        if count == deviceCnt:
				break
                except WriteMetadataError as err:
                        logger.error("%s"%err)
                        count-=1
                        continue
                except MountSwiftDeviceError as err:
                	logger.error("%s"%err)
                        continue

	os.system("mkdir -p /srv/node")
        os.system("chown -R swift:swift /srv/node/")
	logger.debug("end")
        return deviceCnt-count

def readMetadata(disk):
        logger = util.getLogger(name="readMetadata")
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
	logger = util.getLogger(name="getLatestMetadata")
	logger.debug("getLatestMetadata start")
	disks = getNonRootDisks()
	latestMetadata = None

       	for disk in disks:
		(ret, metadata) = readMetadata(disk)
		if ret == 0:
			latestMetadata  = metadata  if latestMetadata is None or latestMetadata["vers"] < metadata["vers"]  else latestMetadata
			

	logger.debug("getLatestMetadata end")
	return latestMetadata

def __loadSwiftMetadata(disk):
        logger = util.getLogger(name="__loadSwiftMetadata")
	metadata = {}
        mountpoint =  "/temp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


	#TODO: chechsum
	if mountDisk(disk, mountpoint) !=0:
                logger.error("Failed to mount %s"%disk)
                return 1

	returncode = 0

	cmd = "cp %s/swift/* /etc/swift/"%(mountpoint)
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
                logger.error("Failed to reload swift metadata from %s for %s"%(disk,output))
		returncode = 1

	if lazyUmount(mountpoint)!=0:
		logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))

	return returncode

def loadSwiftMetadata():
	logger = util.getLogger(name="loadSwiftMetadata")
	logger.info("start")

        disks = getNonRootDisks()
	latestMetadata = getLatestMetadata()
	os.system("mkdir -p /etc/swift")
	returncode = 1

        for disk in disks:
		(ret, metadata) = readMetadata(disk)
		if ret==0 and metadata["vers"] >= latestMetadata["vers"]:
			if __loadSwiftMetadata(disk) ==0:
				returncode = 0
				break

	os.system("chown -R swift:swift /etc/swift")

	logger.info("end")
	return returncode


def remountRecognizableDisks():
	logger = util.getLogger(name="remountRecognizableDisks")
	
        disks = getNonRootDisks()
        unusedDisks = []
	lostDevices =[]

	latest = getLatestMetadata()	
        if latest is None:
                return (lostDevices, disks)

	seenDevices = set()
        for disk in disks:
                (ret, metadata) = readMetadata(disk)
                if ret == 0 and metadata["hostname"] == socket.gethostname() and metadata["vers"] == latest["vers"] and metadata["deviceNum"] not in seenDevices:
			mountpoint = "/srv/node/%s%d"%(metadata["devicePrx"], metadata["deviceNum"])
                        if mountSwiftDevice(disk=disk, devicePrx=metadata["devicePrx"], deviceNum=metadata["deviceNum"]) == 0:
				seenDevices.add(metadata["deviceNum"])
				print "/srv/node/%s%d is back!"%(metadata["devicePrx"], metadata["deviceNum"])
				continue
			else:
                                logger.error("Failed to mount disk %s as swift device %s%d"%(disk, metadata["devicePrx"],metadata["deviceNum"]))

        	unusedDisks.append(disk)

	lostDevices = [x for x in range(1,latest["deviceCnt"]+1) if x not in seenDevices]
	return (lostDevices, unusedDisks)


def resume():
	logger = util.getLogger(name="resume")
        logger.info("start")
	os.system("sh /etc/DCloud/ServerRegister/autoRun.sh")
	hostname = socket.gethostname()

	remountDisks()
	loadSwiftMetadata()	

	util.generateSwiftConfig()
	if util.restartRsync() !=0:
		logger.error("Failed to restart rsync daemon")

	if util.restartMemcached() !=0:
		logger.error("Failed to restart memcached")

	#TODO: check if this node is a proxy node
	os.system("swift-init all restart")

	logger.info("end")

def remountDisks():
	logger = util.getLogger(name="remountDisks")
	logger.info("start")
	
	latest = getLatestMetadata()

	if latest is None:
		return (0, [])

	lazyUmountSwiftDevices()

	(lostDevices, unusedDisks) = remountRecognizableDisks()

	for disk in formatDisks(unusedDisks)[1]:
		if len(lostDevices) == 0:
			break
		if writeMetadata(disk=disk, vers=latest["vers"], deviceCnt=latest["deviceCnt"], devicePrx=latest["devicePrx"], deviceNum=lostDevices[0]) == 0:
			deviceNum = lostDevices[0]
			if mountSwiftDevice(disk=disk, devicePrx=latest["devicePrx"], deviceNum=deviceNum) == 0:
				lostDevices.pop(0)
				print "/srv/node/%s%d is back!"%(latest["devicePrx"], deviceNum)
			else:
				logger.error("Failed to mount disk %s as swift device %s%d "%(disk, latest["devicePrx"], deviceNum))
		else:
			logger.error("Failed to write metadata to %s"%disk)

	logger.info("end")
       	return (len(lostDevices), lostDevices)


def lazyUmount(mountpoint):
        logger = util.getLogger(name="umount")

        returncode = 0
        try:
		if os.path.ismount(mountpoint):
                	cmd = "umount -l %s"%mountpoint
                	po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
			output = po.stdout.read()
                	po.wait()

                	if po.returncode !=0:
                        	logger.error("Failed to umount -l  %s for %s"%(mountpoint, output))
				returncode = 1

        except OSError as e:
                logger.error("Failed to umount -l %s for %s"%(disk, e))
		returncode = 1

        return returncode

def lazyUmountSwiftDevices():
	cmd ="umount -l /srv/node/*"
	os.system(cmd)

def __loadScripts(disk):
        logger = util.getLogger(name="__loadSripts")
	metadata = {}
        mountpoint =  "/temp/%s"%disk
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
	logger = util.getLogger(name="loadSripts")
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

def dumpScripts(destDir):
	logger = util.getLogger(name="dumpScripts")
	os.system("mkdir -p %s"%destDir)
	os.system("rm -r %s/*"%destDir)
        cmd = "cp -r %s/DCloudSwift/* %s"%(BASEDIR,destDir)
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()	
        po.wait()

        if po.returncode != 0:
                logger.error("Failed to dump scripts to %s for %s"%(destDir,output))
                return 1

	return 0

def dumpSwiftMetadata(destDir):
	logger = util.getLogger(name="dumpSwiftMetadata")
	os.system("mkdir -p %s"%destDir)
	os.system("mkdir -p /etc/swift")
        cmd = "cp -r /etc/swift/* %s"%destDir
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
                logger.error("Failed to dump swift metadata to %s for %s"%(destDir,output))
                return 1

	os.system("cd %s; rm -rf %s"%(destDir, UNNECESSARYFILES))
	return 0

def getDeviceMapping():
	logger = util.getLogger(name="getDeviceMapping")

        disks = getNonRootDisks()

	deviceMapping = dict()
	latestMetadata = getLatestMetadata()
	if latestMetadata is None:
		return deviceMapping
	
	for disk in disks:
		(ret, metadata) = readMetadata(disk)
		if ret !=0:
			logger.warn("Failed to read metadata from disk %s"%disk)
			continue

		if metadata["vers"] < latestMetadata["vers"]:
			logger.warn("Metadata of Disk %s is out of date"%disk)
			continue

		if metadata["hostname"] != socket.gethostname():
			logger.warn("Metadata of Disk %s is not valid"%disk)
			continue

		deviceMapping.setdefault(metadata["deviceNum"], disk)

	return deviceMapping

def updateMetadataOnDisks(vers, deviceCnt, devicePrx):
	logger = util.getLogger(name="updateMetadataOnDisks")

	deviceMapping = getDeviceMapping()
	newDeviceMapping = dict()
	for deviceNum in deviceMapping:
		if deviceNum > deviceCnt+1:
			continue

		disk = deviceMapping[deviceNum]
		ret = writeMetadata(disk=disk, vers=vers, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=deviceNum)
		if ret !=0:
			logger.error("Failed to update metadata on disk %s"%disk)
			continue
	
		newDeviceMapping.setdefault(deviceNum, disk)
		logger.info("Succeed to update Metadata on disk %s with deviceNum=%d and vers=%s"%(disk, deviceNum, vers))

		
	return deviceMapping

def writeMetadata(disk, vers, deviceCnt, devicePrx, deviceNum):
	logger = util.getLogger(name="writeMetadata")

	mountpoint =  "/temp/%s"%disk
	os.system("mkdir -p %s"%mountpoint)
	if os.path.ismount(mountpoint):
                        os.system("umount -l %s"%mountpoint)

	cmd = "mount %s %s"%(disk, mountpoint)
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
        	logger.error("Failed to mount %s for %s"%(disk,output))
		return po.returncode


	#TODO: write checksum
	os.system("touch /%s/Metadata"%mountpoint)
	metadata = {"hostname":socket.gethostname(), "vers":vers, "deviceCnt":deviceCnt, "devicePrx":devicePrx, "deviceNum":deviceNum}
	
	try:
		with open("%s/Metadata"%mountpoint, "wb") as fh:
			pickle.dump(metadata, fh)

		if dumpSwiftMetadata("/%s/swift"%mountpoint) !=0:
                	logger.error("Failed to dump swift metadata to %s"%(disk))
                	return 1

		if dumpScripts("/%s/DCloudSwift"%mountpoint) !=0:
			logger.error("Failed to dump scripts to %s"%(disk))
			return 1

		return 0
	except IOError as e:
		logger.error("Failed to wirte metadata for disk %s"%disk)
		return 1
	finally:
		if lazyUmount(mountpoint)!=0:
                        logger.warn("Failed to umount disk %s from %s %s"%(disk, mountpoint))
	

	

def main(argv):
	ret = 0
	if len(argv) > 0:
		if sys.argv[1]=="-r":
			remountDisks()
		elif sys.argv[1]=="-l":
			loadSwiftMetadata()
		elif sys.argv[1]=="-R":
			resume()
		else:
			sys.exit(-1)
	
	return ret

if __name__ == '__main__':
	main(sys.argv[1:])
	#util.generateSwiftConfig()
	#formatDisks(["/dev/sdc"])
	#print getRootDisk()
	#print getAllDisks()
	#print formatNonRootDisks(deviceCnt=3)
	#print getMajorityHostname()
	#print getLatestMetadata()
	#createSwiftDevices()
	#print updateMetadataOnDisks(vers=44, deviceCnt=5, devicePrx="sdb")
	#writeMetadata(disk="/dev/sdb", vers=1, deviceNum=1, devicePrx="sdb", deviceCnt=5)
	#writeMetadata(disk="/dev/sdc", vers=1, deviceNum=2, devicePrx="sdb", deviceCnt=5)
	#writeMetadata(disk="/dev/sdd", vers=1, deviceNum=3, devicePrx="sdb", deviceCnt=5)
	#writeMetadata(disk="/dev/sde", vers=1, deviceNum=4, devicePrx="sdb", deviceCnt=5)
	#writeMetadata(disk="/dev/sdf", vers=1, deviceNum=5, devicePrx="sdb", deviceCnt=5)
	#print loadSwiftMetadata()
	#print updateMetadataOnDisks(vers=44, deviceCnt=5, devicePrx="sdb")
	#print readMetadata(disk="/dev/sdb")
	#print remountDisks()
	#print int(time.time())
	#resume()

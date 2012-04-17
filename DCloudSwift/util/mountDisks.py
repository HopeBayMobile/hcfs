'''
First Created on 2012/03/14 by Ken

'''
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


class MountSwiftDeviceError(Exception): pass
class WriteMetadataError(Exception): pass

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
		

def formatDisks(diskList):
	logger = util.getLogger(name="formatDisks")
	returncode=0
	formattedDisks = []

	for disk in diskList:
                cmd = "mkfs.xfs -i size=1024 -f %s"%(disk)
                po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                po.wait()

                if po.returncode != 0:
                        logger.error("Failed to format %s for %s"%(disk,po.stderr.read()))
                        returncode+=1
                        continue

                formattedDisks.append(disk)


        return (returncode,formattedDisks)


def formatNonRootDisks(deviceCnt=1):
	'''
	Format deviceCnt non-root disks
	'''
	logger = util.getLogger(name="formatNonRootDisks")
	rootDisk = getRootDisk()
	disks = getAllDisks()
	formattedDisks = []
	count=0
	returncode=0

	for disk in disks:
		if disk == rootDisk:
			continue
		count+=1
		if count > deviceCnt:
			break

		cmd = "mkfs.xfs -i size=1024 -f %s"%(disk) 
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()

		if po.returncode != 0:
			logger.error("Failed to format %s for %s"%(disk,po.stderr.readline()))
			returncode+=1
			continue

		formattedDisks.append(disk)


	return (returncode,formattedDisks)


def mountDisk(disk, mountpoint):
	logger = util.getLogger(name="mountDisk")

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

		
def mountSwiftDevice(disk, devicePrx, deviceNum):
        logger = util.getLogger(name="mountSwiftDevice")

        mountpoint = "/srv/node/%s%d"%(devicePrx,deviceNum)
	returncode = mountDisk(disk, mountpoint)

        if returncode != 0:
        	logger.error("Failed to mount %s on %s"%(disk,mountpoint))

	return returncode

def createSwiftDevices(proxyList, deviceCnt=1, devicePrx="sdb"):
        logger = util.getLogger(name="createSwiftDevices")
	(ret,disks)=formatNonRootDisks(deviceCnt)
	if ret != 0:
		return deviceCnt
	
	#TODO: modified to match the ring version
	vers = 0 
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

                        if writeMetadata(disk=disk, vers=vers, proxyList=proxyList, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=count)!=0:
                                raise WriteMetadataError("Failed to write metadata into %s"%disk)

                        if mountSwiftDevice(disk=disk, devicePrx=devicePrx, deviceNum=count)!=0:
                                raise MountSwiftDeviceError("Failed to mount %s on %s"%(disk, mountpoint))

                        if count == deviceCnt:
                                return 0
                except OSError as err:
                        logger.error("Failed to mount %s for %s"%(disk, err))
                        count-=1
                        continue
                except (WriteMetadataError, MountSwiftDeviceError) as err:
                        logger.error("%s"%err)
                        count-=1
                        continue

	os.system("mkdir -p /srv/node")
        os.system("chown -R swift:swift /srv/node/")
        return deviceCnt-count

def readMetadata(disk):
        logger = util.getLogger(name="readMetadata")
	metadata = {}
        mountpoint =  "/temp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


	#TODO: chechsum
	if mountDisk(disk, mountpoint) !=0:
                logger.error("Failed to mount %s"%disk)
                return (1, metadata)

	try:
		with open("%s/Metadata"%mountpoint, "rb") as fh:
			metadata = pickle.load(fh)

		return (0, metadata)
	except IOError as e:
		logger.error("Failed to read metadata from %s for %s"%(disk, e))
		return (1, metadata)
	finally:
		if lazyUmount(mountpoint)!=0:
			logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))


def getLatestMetadata():
	logger = util.getLogger(name="getLatestMetadata")
	rootDisk = getRootDisk()
	disks = getAllDisks()
	latestMetadata = None

       	for disk in disks:
               	if disk == rootDisk:
                       	continue

		(ret, metadata) = readMetadata(disk)
		if ret == 0:
			latestMetadata  = metadata  if latestMetadata is None or latestMetadata["vers"] < metadata["vers"]  else latestMetadata
			

	return latestMetadata

def __loadSwiftMetadata(disk):
        logger = util.getLogger(name="__reloadSwiftMetadata")
	metadata = {}
        mountpoint =  "/temp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


	#TODO: chechsum
	if mountDisk(disk, mountpoint) !=0:
                logger.error("Failed to mount %s"%disk)
                return 1

	returncode = 0

	cmd = "cp %s/swift/*.ring.gz %s/swift/*.builder %s/swift/swift.conf /etc/swift/"%(mountpoint, mountpoint, mountpoint)
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()
        if po.returncode != 0:
                logger.error("Failed to reload swift metadata from %s for %s"%(disk,po.stderr.readline()))
		returncode = 1

	if lazyUmount(mountpoint)!=0:
		logger.warn("Failed to umount disk %s from %s"%(disk, mountpoint))

	return returncode

def loadSwiftMetadata():
	logger = util.getLogger(name="loadSwiftMetadata")
        disks = getNonRootDisks()

        for disk in disks:
		if readMetadata(disk) is not None:
			if __loadSwiftMetadata(disk) !=0:
				os.system("chown -R swift:swift /etc/swift")
				continue
			else:
				return 0

	return 1


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

def remountDisks():
	logger = util.getLogger(name="remountDisks")
	
	latest = getLatestMetadata()

	if latest is None:
		return (0, [])

	lazyUmountSwiftDevices(deviceCnt=latest["deviceCnt"], devicePrx=latest["devicePrx"])

	(lostDevices, unusedDisks) = remountRecognizableDisks()

	for disk in formatDisks(unusedDisks)[1]:
		if len(lostDevices) == 0:
			break
		if writeMetadata(disk=disk, vers=latest["vers"], proxyList=latest["proxyList"], deviceCnt=latest["deviceCnt"], devicePrx=latest["devicePrx"], deviceNum=lostDevices[0]) == 0:
			deviceNum = lostDevices[0]
			if mountSwiftDevice(disk=disk, devicePrx=latest["devicePrx"], deviceNum=deviceNum) == 0:
				lostDevices.pop(0)
				print "/srv/node/%s%d is back!"%(latest["devicePrx"], deviceNum)
			else:
				logger.error("Failed to mount disk %s as swift device %s%d "%(disk, latest["devicePrx"], deviceNum))
		else:
			logger.error("Failed to write metadata to %s"%disk)

       	return (len(lostDevices), lostDevices)


def lazyUmount(mountpoint):
        logger = util.getLogger(name="umount")

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

def lazyUmountSwiftDevices(deviceCnt, devicePrx):
	for deviceNum in range(1,deviceCnt+1):
		lazyUmount("/srv/node/%s%d"%(devicePrx,deviceNum))

def dumpSwiftMetadata(destDir):
	logger = util.getLogger(name="dumpSwiftMetadata")
	os.system("mkdir -p %s"%destDir)
        cmd = "cp /etc/swift/*.ring.gz /etc/swift/*.builder /etc/swift/swift.conf %s"%destDir
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()
        if po.returncode != 0:
                logger.error("Failed to dump swift metadata to %s for %s"%(destDir,po.stderr.read()))
                return po.returncode

def writeMetadata(disk, vers, proxyList,  deviceCnt, devicePrx, deviceNum):
	logger = util.getLogger(name="writeMetadata")

	mountpoint =  "/temp/%s"%disk
	os.system("mkdir -p %s"%mountpoint)
	if os.path.ismount(mountpoint):
                        os.system("umount -l %s"%mountpoint)

	cmd = "mount %s %s"%(disk, mountpoint)
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()
        if po.returncode != 0:
        	logger.error("Failed to mount %s for %s"%(disk,po.stderr.readline()))
		return po.returncode


	#TODO: write checksum
	os.system("touch /%s/Metadata"%mountpoint)
	metadata = {"hostname":socket.gethostname(), "vers":vers, "proxyList":proxyList, "deviceCnt":deviceCnt, "devicePrx":devicePrx, "deviceNum":deviceNum}
	
	try:
		with open("%s/Metadata"%mountpoint, "wb") as fh:
			pickle.dump(metadata, fh)

		if dumpSwiftMetadata("/%s/swift"%mountpoint) !=0:
                	logger.error("Failed to dump swift metadata to %s"%(disk))
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
		else:
			sys.exit(-1)
	else:
		sys.exit(-1)

	
	return ret

if __name__ == '__main__':
	main(sys.argv[1:])
	#print getLatestMetadata()
	#proxyList = [{"ip":"172.16.229.45"}, {"ip":"172.16.229.54"}]
	#createSwiftDevices(proxyList=proxyList, deviceCnt=5)
	#writeMetadata(disk="/dev/sdb", proxyList=proxyList, vers=0, deviceNum=3, devicePrx="sdb", deviceCnt=5)
	#print loadSwiftMetadata()
	#print readMetadata(disk="/dev/sdb")
	#print remountDisks()
	#print int(time.time())
	

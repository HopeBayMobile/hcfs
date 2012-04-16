'''
Created on 2012/03/14

@author: Ken

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

sys.path.append("/DCloudSwift/util")
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

def lazyUnmount(mountpoint):
        logger = util.getLogger(name="lazyUnmount")

        returncode = 1
        try:
                cmd = "umount -l %s"%mountpoint
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                po.wait()

                if po.returncode !=0:
                        logger.error("Failed to umount -l  %s for %s"%(disk, po.stderr.readline()))

                returncode = 0

        except OSError as e:
                logger.error("Failed to umount -l %s for %s"%(disk, e))

        return returncode

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

def createSwiftDevices(deviceCnt=1, devicePrx="sdb"):
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

                        if writeMetadata(disk=disk, vers=vers, deviceCnt=deviceCnt, devicePrx=devicePrx, deviceNum=count)!=0:
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
		if lazyUnmount(mountpoint)!=0:
			logger.warn("Failed to umount disk %s from %s %s"%(disk, mountpoint))


def getLatestMetadata():
	logger = util.getLogger(name="getLatestVersNum")
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


def remountDisks():
	logger = util.getLogger(name="remountDisks")

	rootDisk = getRootDisk()
        disks = getAllDisks()
	unusedDisks = []
	seenSwiftDevices = set()
	swiftDeviceCnt = None
	swiftDevicePrx = None
	blackList = []
	latest = getLatestMetadata()

	if latest is None:
		return (0, [])

       	for disk in disks:
               	if disk == rootDisk:
                       	continue

		(ret, metadata) = readMetadata(disk)
		if ret == 0 and metadata["hostname"] == socket.gethostname() and metadata["vers"] == latest["vers"] and metadata["deviceNum"] not in seenSwiftDevices:
			seenSwiftDevices.add(metadata["deviceNum"])

			if mountSwiftDevice(disk=disk, devicePrx=metadata["devicePrx"], deviceNum=metadata["deviceNum"]) != 0:
				blackList.append(metadata["deviceNum"])
                               	logger.error("Failed to mount disk %s as swift device %s%d"%(disk, metadata["devicePrx"],metadata["deviceNum"]))
			print "%s%d"%(metadata["devicePrx"], metadata["deviceNum"])

		else:
			unusedDisks.append(disk)
		
	lostDeviceNum= [x for x in range(1,latest["deviceCnt"]+1) if x not in seenSwiftDevices]
	for disk in unusedDisks:
		if len(lostDeviceNum) == 0:
			break
		if writeMetadata(disk=disk, vers=latest["vers"], deviceCnt=latest["deviceCnt"], devicePrx=latest["devicePrx"], deviceNum=lostDeviceNum[0]) == 0:
			deviceNum = lostDeviceNum.pop(0)
			seenSwiftDevices.add(deviceNum)
			if mountSwiftDevice(disk=disk, devicePrx=latest["devicePrx"], deviceNum=deviceNum) != 0:
				blackList.append(deviceNum)
				logger.error("Failed to mount disk %s as swift device %s%d "%(disk, latest["devicePrx"], deviceNum))
			print "%s%d"%(metadata["devicePrx"], metadata["deviceNum"])
		else:
			logger.warn("Failed to write metadata to %s"%disk)

       	return (len(blackList), blackList)


def lazyUmount(mountpoint):
        logger = util.getLogger(name="umount")

        returncode = 1
        try:
                cmd = "umount -l %s"%mountpoint
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                po.wait()

                if po.returncode !=0:
                        logger.error("Failed to umount -l  %s for %s"%(disk, po.stderr.readline()))

                returncode = 0
        except OSError as e:
                logger.error("Failed to umount -l %s for %s"%(disk, e))

        return returncode

def writeMetadata(disk, vers, deviceCnt, devicePrx, deviceNum):
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
	metadata = {"hostname":socket.gethostname(), "vers":vers, "deviceCnt":deviceCnt, "devicePrx":devicePrx, "deviceNum":deviceNum}
	
	try:
		with open("%s/Metadata"%mountpoint, "wb") as fh:
			pickle.dump(metadata, fh)	
		return 0
	except IOError as e:
		logger.error("Failed to wirte metadata for disk %s"%disk)
		return 1
	finally:
		if lazyUnmount(mountpoint)!=0:
                        logger.warn("Failed to umount disk %s from %s %s"%(disk, mountpoint))


	

def main(argv):
	ret = 0
	if len(argv) > 0:
		if sys.argv[1]=="-r":
			remountDisks()
		else:
			ret = createSwiftDevices(int(sys.argv[1]))
	else:
		sys.exit(-1)

	
	return ret

if __name__ == '__main__':
	main(sys.argv[1:])
	#writeMetadata(disk="/dev/sdb", deviceNum=3, devicePrx="sdb", deviceCnt=5)
	#print readMetadata(disk="/dev/sdc")
	#print remountDisks()
	#print int(time.time())
	

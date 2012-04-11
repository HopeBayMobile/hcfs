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

                        if writeMetadata(disk, deviceCnt, devicePrx, count)!=0:
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

	deviceCnt=5
	devicePrx="sdb"
	deviceNum=-1

        mountpoint =  "/temp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)


	if mountDisk(disk, mountpoint) !=0:
                logger.error("Failed to mount %s"%disk)
                return (1, deviceCnt, devicePrx, deviceNum)

	try:
		with open("%s/Metadata"%mountpoint, "r") as fh:
			deviceCnt = int(fh.readline().split()[1].strip())
			devicePrx = fh.readline().split()[1].strip()
			deviceNum = int(fh.readline().split()[1].strip())
	except IOError as e:
		logger.error("Failed to read metadata from %s for %s"%(disk, e))
		return(1, deviceCnt, devicePrx, deviceNum)

	if lazyUnmount(mountpoint)!=0:
		logger.warn("Failed to umount disk %s from %s %s"%(disk, mountpoint))

        return (0, deviceCnt, devicePrx, deviceNum)

def remountDisks():
	logger = util.getLogger(name="remountDisks")

	rootDisk = getRootDisk()
        disks = getAllDisks()
	count = 0
	mountedDisks=[]
	returncode = 1

        for disk in disks:
                if disk == rootDisk:
                        continue

		(ret, deviceCnt, devicePrx, deviceNum) = readMetadata(disk)
		print (ret, deviceCnt, devicePrx, deviceNum)
		if ret == 0:
			if mountSwiftDevice(disk=disk, devicePrx=devicePrx, deviceNum=deviceNum) == 0:
				mountedDisks.append(disk)
				if len(mountedDisks) == deviceCnt:
					returncode = 0

				

        return (returncode, mountedDisks)


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

def writeMetadata(disk, deviceCnt, devicePrx, deviceNum):
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

	os.system("touch /%s/Metadata"%mountpoint)
	line1 = "deviceCnt %d"%deviceCnt
	line2 = "devicePrx %s"%devicePrx
	line3 = "deviceNum %d"%deviceNum
	os.system("echo \"%s\" > /temp/%s/Metadata"%(line1,disk)) 
	os.system("echo \"%s\" >> /temp/%s/Metadata"%(line2,disk)) 
	os.system("echo \"%s\" >> /temp/%s/Metadata"%(line3,disk)) 
	os.system("umount %s"%mountpoint)

	return 0

	

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
#	(ret, sn) = getDiskSN("/dev/sda")
#	print sn
#	writeMetadata(disk="/dev/sdb", deviceNum=1, devicePrx="sdb", deviceCnt=5)
	#print readMetadata(disk="/dev/sdb")
#	print remountDisks()
	

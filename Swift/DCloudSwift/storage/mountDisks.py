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

def getDiskSN(disk=""):
        '''
        get disk serial number
        '''
        logger = util.getLogger(name="getDiskSN")

        cmd = "hdparm -I %s | grep \"Serial Number\""%disk
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()

        if po.returncode != 0:
        	logger.error("Failed to get SN of %s for %s"%(disk,po.stderr.readline()))
		return (po.returncode,"")


	SN = po.stdout.readline()
	SN = SN.split(':')[1].strip()
	
        return (0,SN)


def formatNonRootDisks(deviceCnt=1):
	'''
	Format the first deviceCnt non-root disks
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

		cmd = "umount %s; mkfs.xfs -i size=1024 -f %s"%(disk,disk) 
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()

		if po.returncode != 0:
			logger.error("Failed to format %s for %s"%(disk,po.stderr.readline()))
			returncode+=1
			continue

		formattedDisks.append(disk)


	return (returncode,formattedDisks)
'''
def mountFormattedDisks(disks=[]):
	logger = util.getLogger(name="mountFormattedDisks")
	returncode = 0
	count = 0

	for disk in disks:
		count+=1
		mountpoint = "/srv/node"+"/sdb%d"%count
		os.system("mkdir -p %s"%mountpoint)
		if os.path.ismount(mountpoint):
			os.system("umount -l %s"%mountpoint)

		line = "%s %s xfs noatime,nodiratime,nobarrier,logbufs=8 0 0"%(disk, mountpoint)
		if not util.findLine("/etc/fstab", line):
			cmd = "echo \"%s\" >>/etc/fstab\n"%line
			os.system(cmd)

		cmd = "mount %s"%(mountpoint)
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()

		if po.returncode != 0:
			logger.error("Failed to mount %s for %s"%(disk,po.stderr.readlines()))
			returncode+=1
			continue
	return returncode
'''
		
def mountSingleFormattedDisk(disk, devicePrx, deviceNum):
        logger = util.getLogger(name="mountSingleFormattedDisk")

        mountpoint = "/srv/node/%s%d"%(devicePrx,deviceNum)
        os.system("mkdir -p %s"%mountpoint)
        if os.path.ismount(mountpoint):
        	os.system("umount -l %s"%mountpoint)

        cmd = "mount %s %s"%(disk, mountpoint)
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()

        if po.returncode != 0:
        	logger.error("Failed to mount %s for %s"%(disk,po.stderr.readlines()))

	return po.returncode

def mountFormattedDisks(disks=[], deviceCnt=5, devicePrx="sdb"):
        logger = util.getLogger(name="mountFormattedDisks")

        count = 0
        for disk in disks:
		try:
                	count+=1
                	mountpoint = "/srv/node"+"/%s%d"%(devicePrx,count)
                	os.system("mkdir -p %s"%mountpoint)
                	if os.path.ismount(mountpoint):
                        	os.system("umount -l %s"%mountpoint)

                #line = "%s %s xfs noatime,nodiratime,nobarrier,logbufs=8 0 0"%(disk, mountpoint)
                #if not util.findLine("/etc/fstab", line):
                 #       cmd = "echo \"%s\" >>/etc/fstab\n"%line
                  #      os.system(cmd)
		
			if writeMetadata(disk, deviceCnt, devicePrx, count)!=0:
				raise

			if mountSingleFormattedDisk(disk=disk, devicePrx=devicePrx, deviceNum=count)!=0:
				raise

			if count == deviceCnt:
				return 0
		except Exception as e:
			logger.error("Failed to mount %s for %s"%(disk, e))
			count-=1
			continue

        return deviceCnt-count

def prepareMountPoints(deviceCnt=2, devicePrx="sdb"):
	(ret,disks)=formatNonRootDisks(deviceCnt)
	if ret != 0:
		return ret
	
	os.system("chown -R swift:swift /srv/node/")
	return mountFormattedDisks(disks, deviceCnt=deviceCnt, devicePrx=devicePrx)

def readMetadata(disk):
        logger = util.getLogger(name="readMetadata")

	deviceCnt=5
	devicePrx="sdb"
	deviceNum=-1

        mountpoint =  "/temp/%s"%disk
        os.system("mkdir -p %s"%mountpoint)
        if os.path.ismount(mountpoint):
                        os.system("umount -l %s"%mountpoint)

        cmd = "mount %s %s"%(disk, mountpoint)
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        po.wait()
        if po.returncode != 0:
                logger.error("Failed to mount %s for %s"%(disk,po.stderr.readlines()))
                return (po.returncode, deviceCnt, devicePrx, deviceNum)

	try:
		fh = open("%s/Metadata"%mountpoint, "r")

		deviceCnt = int(fh.readline().split()[1].strip())
		devicePrx = fh.readline().split()[1].strip()
		deviceNum = int(fh.readline().split()[1].strip())
		fh.close()
	except Exception as e:
		logger.error("Failed to read metadata from %s for %s"%(disk, e))
		return(1, deviceCnt, devicePrx, deviceNum)

	os.system("umount %s"%mountpoint)
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
			if mountSingleFormattedDisk(disk=disk, devicePrx=devicePrx, deviceNum=deviceNum) == 0:
				mountedDisks.append(disk)
				if len(mountedDisks) == deviceCnt:
					returncode = 0

				

        return (returncode, mountedDisks)


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
        	logger.error("Failed to mount %s for %s"%(disk,po.stderr.readlines()))
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
			ret = prepareMountPoints(int(sys.argv[1]))
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
	

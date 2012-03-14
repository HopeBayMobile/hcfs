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

def formatNonRootDisks(deviceCnt=1):
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
			logger.error("Failed to format %s for %s"%(disk,po.stderr.readlines()))
			returncode+=1
			continue

		formattedDisks.append(disk)


	return (returncode,formattedDisks)

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
		
def prepareMountPoints(deviceCnt=1):
	(ret,disks)=formatNonRootDisks(deviceCnt)
	if ret != 0:
		return ret
	
	return mountFormattedDisks(disks)

def main(argv):
	ret = 0
	if len(argv) > 0:
		ret = prepareMountPoints(int(sys.argv[1]))
	else:
		sys.exit(-1)

	
	return ret

if __name__ == '__main__':
	main(sys.argv[1:])

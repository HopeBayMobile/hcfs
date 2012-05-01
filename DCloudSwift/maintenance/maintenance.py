import os, sys, subprocess
import fcntl
import threading
import random
import signal
import socket
import struct
import math
import pickle
import time

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)
sys.path.append("%s/DCloudSwift/util"%BASEDIR)

import util
import mountDisks
from ConfigParser import ConfigParser
from SwiftCfg import SwiftCfg

SWIFTCONF = '%s/DCloudSwift/Swift.ini'%BASEDIR
FORMATTER = '[%(levelname)s from %(name)s on %(asctime)s] %(message)s'


def isNewer(confDir):
	logger = util.getLogger(name="checkSwiftConfVers")
	oriVers = util.getSwiftConfVers(confDir="/etc/swift/")
	vers = util.getSwiftConfVers(confDir=confDir)
	
	

	if vers <= oriVers:
		return False
	
	return True	

def updateMetadata(confDir):
	logger = util.getLogger(name="updateMetadata")
	
	os.system("rm -rf /etc/swift")
	os.system("rm -rf /DCloudSwift")
	os.system("mkdir -p /etc/swift")
	os.system("cp -r %s/* /etc/swift/"%confDir)
	os.system("cp -r %s/DCloudSwift /"%BASEDIR)
	os.system("chown -R swift:swift /etc/swift")

	deviceCnt = util.getDeviceCnt()
	devicePrx = util.getDevicePrx()

	oriVers = mountDisks.getLatestVers()
	if oriVers is None:
		mountDisks.createSwiftDevices(deviceCnt=deviceCnt,devicePrx=devicePrx)
	else:
		mountDisks.updateMetadataOnDisks(oriVers=oriVers)
		mountDisks.mountUmountedSwiftDevices()
		

	util.restartAllServices()
	return 0



if __name__ == '__main__':
#	print isNewer(confDir="/etc/delta/swift")
	updateMetadata(confDir="/etc/delta/swift")
#	print jsonStr2SshpassArg('{ "Hello" : 3, "list":["192.167.1.1", "178.16.3.1"]}')
#	spreadPackages(password="deltacloud", nodeList = ["172.16.229.24"])
#	print installAllDeb("/DCloudSwift/storage/deb_source")
#	print isLineExistent("/etc/fstab","ddd")
#	print getStorageNodeIpList()
#	runPopenCommunicate("cd /etc/swift", inputString='y\n', logger=logger)
#	runPopenCommunicate("mkfs -t ext4 /dev/sda", inputString='y\n', logger=logger)

#	logger = getLogger(name="Hello")
#	logger.info("Hello")

#	spreadMetadata(password="deltacloud",nodeList=["172.16.229.132"])

#	@timeout(5)
#	def printstring():
#		print "Start!!"
#		time.sleep(10)
#		print "This is not timeout!!!"
#	printstring()
	#sendMaterials("deltacloud", "172.16.229.146")
	#cmd = "ssh root@172.16.229.146 sleep 5"
	#print sshpass("deltacloud", cmd, timeout=1)
#	print os.path.dirname(os.path.dirname(os.getcwd()))
	pass	

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

from util import util
from util import mountDisks

def isNewer(confDir):
	logger = util.getLogger(name="isNewer")
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
		util.stopAllServices() #prevent services from occupying disks
		mountDisks.createSwiftDevices(deviceCnt=deviceCnt,devicePrx=devicePrx)
	else:
		mountDisks.updateMetadataOnDisks(oriVers=oriVers)
		mountDisks.mountUmountedSwiftDevices()
		
	util.restartAllServices()
	return 0



if __name__ == '__main__':
	pass	

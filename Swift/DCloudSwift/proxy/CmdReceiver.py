'''
Created on 2012/03/01

@author: CW

Modified by Ken on 2012/03/12
Modified by Ken on 2012/03/13
Modified by CW on 2012/03/22: correct the absolute path of function triggerProxyDeploy()
'''

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import shlex
import random
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

sys.path.append("/DCloudSwift/util")
import util

Usage = '''
Usage:
	python CmdReceiver.py [Option] [jsonStr]
Options:
	[-a | addStorage] - for adding storage nodes
	[-p | proxy] - for proxy node
	[-r | rmStorage] - for removing storage nodes
Examples:
	python CmdReceiver.py -p 
'''

def usage():
	print Usage
	sys.exit(1)

def triggerAddStorage(**kwargs):
	logger = util.getLogger(name="triggerAddStorage")
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	devicePrx = kwargs['devicePrx']
	deviceCnt = kwargs['deviceCnt']
	password = kwargs['password']

	random.seed(time.time())
	for i in storageList: 
		zoneNumber= random.randint(1,100)
		for j in range(deviceCnt):
			deviceName = devicePrx + str(j+1)
			logger.info("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (zoneNumber, i, deviceName))
			os.system("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (zoneNumber, i, deviceName))

	os.system("/DCloudSwift/proxy/Rebalance.sh")
	os.system("cp --preserve /etc/swift/*.ring.gz /tmp/")

	blackProxyNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=proxyList)

	allStorageNodes = util.getStorageNodeIpList()
	blackStorageNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=allStorageNodes)

	returncode = len(blackProxyNodes)+len(blackStorageNodes)

	return (returncode, blackProxyNodes, blackStorageNodes)


def triggerProxyDeploy(**kwargs):
	logger = util.getLogger(name = "triggerProxyDeploy")
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceCnt = kwargs['deviceCnt']
	devicePrx = kwargs['devicePrx']
	os.system("/DCloudSwift/proxy/PackageInstall.sh %d" % numOfReplica)
	zoneNumber = 1
	for i in storageList: 
		for j in range(deviceCnt):
			deviceName = devicePrx + str(j+1)
			logger.info("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (zoneNumber, i, deviceName))
			os.system("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (zoneNumber, i, deviceName))
			zoneNumber += 1
	os.system("/DCloudSwift/proxy/ProxyStart.sh")

def triggerRmStorage(**kwargs):
	logger = util.getLogger(name="triggerRmStorage")
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	password = kwargs['password']

	for i in storageList: 
		cmd = "cd /etc/swift; swift-ring-builder account.builder remove %s"% (i)
		util.runPopenCommunicate(cmd, inputString='y\n', logger=logger)

		cmd = "cd /etc/swift; swift-ring-builder container.builder remove %s"% (i)
		util.runPopenCommunicate(cmd, inputString='y\n', logger=logger)

		cmd = "cd /etc/swift; swift-ring-builder object.builder remove %s"% (i)
		util.runPopenCommunicate(cmd, inputString='y\n', logger=logger)

	os.system("/DCloudSwift/proxy/Rebalance.sh")
	os.system("cp --preserve /etc/swift/*.ring.gz /tmp/")

	blackProxyNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=proxyList)

	allStorageNodes = util.getStorageNodeIpList()
	blackStorageNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=allStorageNodes)

	returncode = len(blackProxyNodes)+len(blackStorageNodes)

	return (returncode, blackProxyNodes, blackStorageNodes)

def main():
	if not util.isAllDebInstalled("/DCloudSwift/proxy/deb_source/"):
		util.installAllDeb("/DCloudSwift/proxy/deb_source/")

	if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
		os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	if (len(sys.argv) == 3 ):
		kwargs = None
        	if (sys.argv[1] == 'addStorage' or sys.argv[1] == '-a'):
			try:
				kwargs = json.loads(sys.argv[2])
			except ValueError:
				print "Usage error: Ivalid json format"
				usage()

			print 'AddStorage start'
			(returncode, blackProxy, blackStorage) = triggerAddStorage(**kwargs)
		elif (sys.argv[1] == 'rmStorage' or sys.argv[1] == '-r'):
                        f = file('/DCloudSwift/proxy/RmStorageParams', 'r')
                        kwargs = f.readline()

                        try:
                                kwargs = json.loads(kwargs)
                        except ValueError:
                                print "Usage error: Ivalid json format"
                                usage()

                        print 'Proxy deployment start'
                        triggerRmStorage(**kwargs)	

			
	elif (len(sys.argv) == 2):

        	if (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
			f = file('/DCloudSwift/proxy/ProxyParams', 'r')
			kwargs = f.readline()

			try:
				kwargs = json.loads(kwargs)
			except ValueError:
				print "Usage error: Ivalid json format"
				usage()

			print 'Proxy deployment start'
			triggerProxyDeploy(**kwargs)
		else:
			print "Usage error: Invalid optins"
                	usage()
        else:
		#print len(sys.argv)
		#for i in range(0, len(sys.argv)):
		#	print sys.argv[i]
		usage()


if __name__ == '__main__':
	main()
	print "End of executing CmdReceiver.py"

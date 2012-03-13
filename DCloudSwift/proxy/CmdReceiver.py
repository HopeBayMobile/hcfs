'''
Created on 2012/03/01

@author: CW

Modified by Ken on 2012/03/12
Modified by Ken on 2012/03/13
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
	python CmdReceiver.py [Option]
Options:
	[-a | addStorage] - for adding storage nodes
	[-p | proxy] - for proxy node
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
	deviceName = kwargs['deviceName']
	password = kwargs['password']

		

	random.seed(time.time())
	for i in storageList: 
		zoneNumber= random.randint(1,100000)
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
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceName = kwargs['deviceName']
	os.system("/DCloudSwift/proxy/PackageInstall.sh %d" % numOfReplica)
	zoneNumber = 1
	for i in storageList: 
		os.system("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (zoneNumber, i, deviceName))
		zoneNumber += 1
	os.system("/DCloudSwift/proxy/ProxyStart.sh")

def main():
	if (len(sys.argv) == 2 ):
		kwargs = None
        	if (sys.argv[1] == 'addStorage' or sys.argv[1] == '-a'):
			f = file('/DCloudSwift/proxy/AddStorageParams', 'r')
			kwargs = f.readline()

			try:
				kwargs = json.loads(kwargs)
			except ValueError:
				print "Usage error: Ivalid json format"
				usage()

			print 'AddStorage start'
			(returncode, blackProxy, blackStorage) = triggerAddStorage(**kwargs)

			

        	elif (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
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

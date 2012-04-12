'''
Created on 2012/03/01

@author: CW

Modified by Ken on 2012/03/12
Modified by Ken on 2012/03/13
Modified by CW on 2012/03/22: correct the absolute path of function triggerProxyDeploy()
Modified by Ken on 2012/04/09: add triggerFirstProxy
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
	[-f | firstProxy] - for the first proxy node
Examples:
	python CmdReceiver.py -p {"password": "deltacloud"}
'''

def usage():
	print >> sys.stderr, Usage
	sys.exit(1)


def triggerAddStorage(**kwargs):
	logger = util.getLogger(name="triggerAddStorage")
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	devicePrx = kwargs['devicePrx']
	deviceCnt = kwargs['deviceCnt']
	password = kwargs['password']

	for node in storageList: 
		for j in range(deviceCnt):
			deviceName = devicePrx + str(j+1)
			logger.info("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (node["zid"], node["ip"], deviceName))
			os.system("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (node["zid"], node["ip"], deviceName))

	os.system("/DCloudSwift/proxy/Rebalance.sh")
	os.system("cp --preserve /etc/swift/*.ring.gz /tmp/")

	blackProxyNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=[node["ip"] for node in proxyList])

	allStorageNodes = util.getStorageNodeIpList()
	blackStorageNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=allStorageNodes)

	returncode = len(blackProxyNodes)+len(blackStorageNodes)

	return (returncode, blackProxyNodes, blackStorageNodes)


def triggerFirstProxyDeploy(**kwargs):
	logger = util.getLogger(name = "triggerFirstProxyDeploy")
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceCnt = kwargs['deviceCnt']
	devicePrx = kwargs['devicePrx']
	os.system("/DCloudSwift/proxy/CreateProxyConfig.sh")
	os.system("/DCloudSwift/proxy/CreateRings.sh %d" % numOfReplica)
	zoneNumber = 1
	for node in storageList: 
		for j in range(deviceCnt):
			deviceName = devicePrx + str(j+1)
			logger.info("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (node["zid"], node["ip"], deviceName))
			os.system("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (node["zid"], node["ip"], deviceName))

	os.system("/DCloudSwift/proxy/Rebalance.sh")
	os.system("/DCloudSwift/proxy/ProxyStart.sh")
	return 0

def triggerProxyDeploy(**kwargs):
	logger = util.getLogger(name = "triggerProxyDeploy")
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceCnt = kwargs['deviceCnt']
	devicePrx = kwargs['devicePrx']
	os.system("/DCloudSwift/proxy/CreateProxyConfig.sh")
	os.system("/DCloudSwift/proxy/ProxyStart.sh")
	return 0

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

	blackProxyNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=[node["ip"] for node in proxyList])

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
				print >> sys.stderr, "Usage error: Ivalid json format"
				usage()

			print 'AddStorage start'
			(returncode, blackProxy, blackStorage) = triggerAddStorage(**kwargs)
		elif (sys.argv[1] == 'rmStorage' or sys.argv[1] == '-r'):
                        try:
                                kwargs = json.loads(sys.argv[2])
                        except ValueError:
                                print >> sys.stderr, "Usage error: Ivalid json format"
                                usage()

                        print 'Proxy deployment start'
                        triggerRmStorage(**kwargs)	

        	elif (sys.argv[1] == 'firstProxy' or sys.argv[1] == '-f'):
			try:
				kwargs = json.loads(sys.argv[2])
			except ValueError:
				print >>sys.stderr,  "Usage error: Ivalid json format"
				usage()

			print 'First proxy deployment start'
			triggerFirstProxyDeploy(**kwargs)

        	elif (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
			try:
				kwargs = json.loads(sys.argv[2])
			except ValueError:
				print >>sys.stderr,  "Usage error: Ivalid json format"
				usage()

			print 'Proxy deployment start'
			triggerProxyDeploy(**kwargs)
		else:
			print >> sys.stderr, "Usage error: Invalid optins"
                	usage()
        else:
		usage()


if __name__ == '__main__':
	main()
	print "End of executing CmdReceiver.py"

'''
Created on 2012/03/01

@author: CW

Modified by Ken on 2012/03/12
'''

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import shlex
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser


Usage = '''
Usage:
	python CmdReceiver.py [Option]
Options:
	[-a | addStorage] - for adding storage nodes
	[-p | proxy] - for proxy node
	[-s | storage] - for storage node
Examples:
	python CmdReceiver.py -p 
'''

def usage():
	print Usage
	sys.exit(1)

def triggerAddStorage(**kwargs):
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceName = kwargs['deviceName']
#	os.system("/proxy/PackageInstall.sh %d" % numOfReplica)
#	zoneNumber = 1
#	for i in storageList: 
#		os.system("/proxy/AddRingDevice.sh %d %s %s" % (zoneNumber, i, deviceName))
#		zoneNumber += 1
#	os.system("/proxy/ProxyStart.sh")
	os.system("touch /tmp/Hello")

def triggerProxyDeploy(**kwargs):
	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceName = kwargs['deviceName']
	os.system("/proxy/PackageInstall.sh %d" % numOfReplica)
	zoneNumber = 1
	for i in storageList: 
		os.system("/proxy/AddRingDevice.sh %d %s %s" % (zoneNumber, i, deviceName))
		zoneNumber += 1
	os.system("/proxy/ProxyStart.sh")

def triggerStorageDeploy(**kwargs):
	proxyNode = kwargs['proxyList'][0]
	deviceName = kwargs['deviceName']
	os.system("/storage/StorageInstall.sh %s %s" % (proxyNode, deviceName))
	
def main():
	if (len(sys.argv) == 2 ):
		kwargs = None
        	if (sys.argv[1] == 'addStorage' or sys.argv[1] == '-a'):
			f = file('/proxy/AddStorageParams', 'r')
			kwargs = f.readline()

			try:
				kwargs = json.loads(kwargs)
			except ValueError:
				print "Usage error: Ivalid json format"
				usage()

			print 'AddStorage start'
			triggerAddStorage(**kwargs)

        	elif (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
			f = file('/proxy/ProxyParams', 'r')
			kwargs = f.readline()

			try:
				kwargs = json.loads(kwargs)
			except ValueError:
				print "Usage error: Ivalid json format"
				usage()

			print 'Proxy deployment start'
			triggerProxyDeploy(**kwargs)
		elif (sys.argv[1] == 'storage' or sys.argv[1] == '-s'):
			f = file('/storage/StorageParams', 'r')
			kwargs = f.readline()

			try:
				kwargs = json.loads(kwargs)
			except ValueError:
				print "Usage error: Ivalid json format"
				usage()

			print 'storage deployment start'
			triggerStorageDeploy(**kwargs)
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

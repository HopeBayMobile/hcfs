import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import shlex
import random
import fcntl
import pickle
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

sys.path.append("/DCloudSwift/util")
import util
import mountDisks

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
EEXIST = 17
lockFile = "/tmp/CmdReceiver.lock"

class UsageError(Exception):
	pass

def usage():
	print >> sys.stderr, Usage


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


def triggerProxyDeploy(**kwargs):
	logger = util.getLogger(name = "triggerProxyDeploy")
	logger.info("triggerProxyDeploy start")

	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceCnt = kwargs['deviceCnt']
	devicePrx = kwargs['devicePrx']
	os.system("/DCloudSwift/proxy/CreateProxyConfig.sh")
	os.system("/DCloudSwift/proxy/ProxyStart.sh")
	logger.info("Proxy started")
	metadata = mountDisks.getLatestMetadata()
	
	if metadata is None or metadata["vers"] < util.getSwiftConfVers():
		mountDisks.createSwiftDevices(deviceCnt=deviceCnt,devicePrx=devicePrx)
	else:
		mountDisks.remountDisks()

	looger.info("triggerProxyDeploy end")
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
	returncode =0
	fd = -1
	try:
		fd = os.open(lockFile, os.O_RDWR| os.O_CREAT | os.O_EXCL, 0444)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

		if (len(sys.argv) == 3 ):
			kwargs = None
        		if (sys.argv[1] == 'addStorage' or sys.argv[1] == '-a'):
				kwargs = json.loads(sys.argv[2])
				print 'AddStorage start'
				triggerAddStorage(**kwargs)
			elif (sys.argv[1] == 'rmStorage' or sys.argv[1] == '-r'):
                               	kwargs = json.loads(sys.argv[2])
                        	print 'Proxy deployment start'
                        	triggerRmStorage(**kwargs)	
        		elif (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
				kwargs = json.loads(sys.argv[2])
				print 'Proxy deployment start'
				triggerProxyDeploy(**kwargs)
			else:
				print >> sys.stderr, "Usage error: Invalid optins"
                		raise UsageError
        	else:
			raise UsageError
	except OSError as e:
		if e.errno == EEXIST:
			print >>sys.stderr, "A confilct task is in execution"
		else:
			print >>sys.stderr, str(e)
		returncode = e.errno
	except UsageError:
		usage()
		returncode = 1
	except ValueError:
		print >>sys.stderr,  "Usage error: Ivalid json format"
		returncode = 1
	except Exception as e:
		print >>sys.stderr, str(e)
		returncode = 1
	finally:
		if fd != -1:
			os.close(fd)
			os.unlink(lockFile)
		sys.exit(returncode)


if __name__ == '__main__':
	main()
	print "End of executing CmdReceiver.py"

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

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(WORKING_DIR)
os.chdir(WORKING_DIR)
sys.path.append("%s/DCloudSwift/"%BASEDIR)

from util import util
from util import mountDisks
from maintenance import maintenance
from storage import StorageInstall

Usage = '''
Usage:
	python CmdReceiver.py [Option] [jsonStr]
Options:
	[-a | addStorage] - for adding storage nodes
	[-p | proxy] - for proxy node
	[-r | rmStorage] - for removing storage nodes
	[-u | updateMetadata]
	[-s | storage] - for storage node
Examples:
	python CmdReceiver.py -p {"password": "deltacloud"}
'''
EEXIST = 17
lockFile = "/etc/delta/swift.lock"

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
			cmd = "%s/DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (BASEDIR, node["zid"], node["ip"], deviceName)
			logger.info(cmd)
			os.system(cmd)

	os.system("sh %s/DCloudSwift/proxy/Rebalance.sh"%BASEDIR)
	os.system("cp --preserve /etc/swift/*.ring.gz /tmp/")

	blackProxyNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=[node["ip"] for node in proxyList])

	allStorageNodes = util.getStorageNodeIpList()
	blackStorageNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=allStorageNodes)

	returncode = len(blackProxyNodes)+len(blackStorageNodes)

	return (returncode, blackProxyNodes, blackStorageNodes)


def triggerUpdateMetadata(confDir):
	logger = util.getLogger(name="triggerUpdateMetadata")
	logger.info("start")

	if not maintenance.isNewer(confDir=confDir):
		logger.info("Already the latest metadata")
	else:
		maintenance.updateMetadata(confDir=confDir)

	logger.info("end")
	return 0

def triggerProxyDeploy(**kwargs):
	logger = util.getLogger(name = "triggerProxyDeploy")
	logger.info("start")

	proxyList = kwargs['proxyList']
	storageList = kwargs['storageList']
	numOfReplica = kwargs['numOfReplica']
	deviceCnt = kwargs['deviceCnt']
	devicePrx = kwargs['devicePrx']

	util.generateSwiftConfig()
	util.restartMemcached()
	os.system("sh %s/DCloudSwift/proxy/ProxyStart.sh"%BASEDIR)

	metadata = mountDisks.getLatestMetadata()
	
	if metadata is None or metadata["vers"] < util.getSwiftConfVers():
		ret = mountDisks.createSwiftDevices(deviceCnt=deviceCnt,devicePrx=devicePrx)
		if ret !=0:
			logger.warn("Failed to create all swift devices")
	else:
		mountDisks.remountDisks()

	logger.info("end")
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

	os.system("sh %s/DCloudSwift/proxy/Rebalance.sh"%BASEDIR)
	os.system("cp --preserve /etc/swift/*.ring.gz /tmp/")

	blackProxyNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=[node["ip"] for node in proxyList])

	allStorageNodes = util.getStorageNodeIpList()
	blackStorageNodes = util.spreadMetadata(password=password, sourceDir="/tmp/", nodeList=allStorageNodes)

	returncode = len(blackProxyNodes)+len(blackStorageNodes)

	return (returncode, blackProxyNodes, blackStorageNodes)

def triggerStorageDeploy(**kwargs):
	proxy = kwargs['proxyList'][0]["ip"]
	proxyList = kwargs['proxyList']

	devicePrx = kwargs['devicePrx']
	deviceCnt = kwargs['deviceCnt']
	installer = StorageInstall.StorageNodeInstaller(proxy=proxy, proxyList=proxyList, devicePrx=devicePrx, deviceCnt=deviceCnt)
	installer.install()

def main():
	returncode =0
	fd = -1

	try:
		os.system("mkdir -p %s"%os.path.dirname(lockFile))
		fd = os.open(lockFile, os.O_RDWR| os.O_CREAT | os.O_EXCL, 0444)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

		if (len(sys.argv) == 3 ):
			kwargs = None
        		if (sys.argv[1] == 'addStorage' or sys.argv[1] == '-a'):
				kwargs = json.loads(sys.argv[2])
				print 'AddStorage start'
				triggerAddStorage(**kwargs)
        		elif (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
				kwargs = json.loads(sys.argv[2])
				print 'Proxy deployment start'
				triggerProxyDeploy(**kwargs)
			elif (sys.argv[1] == 'rmStorage' or sys.argv[1] == '-r'):
                               	kwargs = json.loads(sys.argv[2])
                        	print 'RmStorage deployment start'
                        	triggerRmStorage(**kwargs)	
			elif (sys.argv[1] == 'storage' or sys.argv[1] == '-s'):
				kwargs = json.loads(sys.argv[2])
				print 'storage deployment start'
				triggerStorageDeploy(**kwargs)
        		elif (sys.argv[1] == 'updateMetadata' or sys.argv[1] == '-u'):
				print 'updateMetadata start'
				confDir = sys.argv[2] 
				triggerUpdateMetadata(confDir=confDir)
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

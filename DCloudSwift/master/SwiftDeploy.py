import os, sys, socket
import posixfile
import time
import json
import subprocess
import threading
import threadpool
import datetime
import logging
import pickle
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

#Self defined packages
sys.path.append("/DCloudSwift/util")
from SwiftCfg import SwiftCfg
import util
from util import timeout


#TODO: read from config files
UNNECESSARYFILES = "cert* backups"

class SwiftDeploy:
	def __init__(self, proxyList = [], storageList = []):
		self.__proxyList = proxyList
		self.__storageList = storageList
		self.__mentor = proxyList[0]

		self.__SC = SwiftCfg("/DCloudSwift/Swift.ini")
		self.__kwparams = self.__SC.getKwparams()
		self.__kwparams['proxyList'] = self.__proxyList
		self.__kwparams['storageList'] = self.__storageList
		self.__cnt = self.__kwparams['deviceCnt'] 

		self.__deployProgress = {
			'proxyProgress': 0,
			'storageProgress': 0,
			'finished': False,
			'code': 0,
			'deployedProxy': 0,
			'deployedStorage': 0,
			'blackList': [],
			'message': []
		}

		os.system("mkdir -p %s" % self.__kwparams['logDir'])
		os.system("mkdir -p %s" % self.__kwparams['reportDir'])

		self.__jsonStr = json.dumps(self.__kwparams)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def getDeployProgress(self):
		return self.__deployProgress

	def __updateProgress(self, success=False, ip="", swiftType="proxy", msg=""):
		if swiftType == "proxy":
			self.__deployProgress['deployedProxy'] += 1
			self.__deployProgress['proxyProgress'] = (self.__deployProgress['deployedProxy']/len(self.__proxyList)) * 100.0
		if swiftType == "storage":
			self.__deployProgress['deployedStorage'] += 1
			self.__deployProgress['storageProgress'] = (self.__deployProgress['deployedStorage']/len(self.__storageList)) * 100.0
		if success == False:
			self.__deployProgress['blackList'].append(ip)
			self.__deployProgress['message'].append(msg)
			self.__deployProgress['code'] += 1
		if self.__deployProgress['deployedProxy'] == len(self.__proxyList) and self.__deployProgress['deployedStorage'] == len(self.__storageList):
			self.__deployProgress['finished'] = True

	def createMetadata(self):
		logger = util.getLogger(name = "createMetadata")
		proxyList = self.__kwparams['proxyList']
		storageList = self.__kwparams['storageList']
		numOfReplica = self.__kwparams['numOfReplica']
		deviceCnt = self.__kwparams['deviceCnt']
		devicePrx = self.__kwparams['devicePrx']
		versBase = int(time.time())*100000

		os.system("mkdir -p /etc/delta/swift")
		os.system("mkdir -p /etc/swift")
		os.system("touch /etc/swift/proxyList")
		os.system("touch /etc/swift/versBase")
		with open("/etc/swift/proxyList", "wb") as fh:
			pickle.dump(proxyList, fh)
			
		with open("/etc/swift/versBase", "wb") as fh:
			pickle.dump(versBase, fh)

		os.system("sh /DCloudSwift/proxy/CreateRings.sh %d" % numOfReplica)
		zoneNumber = 1
		for node in storageList: 
			for j in range(deviceCnt):
				deviceName = devicePrx + str(j+1)
				logger.info("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (node["zid"], node["ip"], deviceName))
				os.system("sh /DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (node["zid"], node["ip"], deviceName))

		os.system("sh /DCloudSwift/proxy/Rebalance.sh")

		os.system("cp -r /etc/swift /etc/delta")
		os.system("rm -rf /etc/swift/*")
		os.system("cd /etc/delta/swift; rm -rf %s"%UNNECESSARYFILES)

	#@timeout(self.__kwparams['proxyInterval'], -1)
	def __proxyDeploySubtask(self, proxyIP):
		logger = util.getLogger(name="proxyDeploySubtask: %s" % proxyIP)
		try:
			if util.spreadMetadata(password=self.__kwparams['password'], sourceDir='/etc/delta/swift', nodeList=[proxyIP])[0] != 0:
				errMsg = "Failed to spread metadata to %s" % proxyIP
				logger.error(errMsg)
				self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=errMsg)
				return -2

			cmd = "scp -r /DCloudSwift/ root@%s:/" % proxyIP
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp proxy deploy scripts to %s for %s" % (proxyIP, stderr)
				logger.error(errMsg)
				self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=errMsg)
				return -3

			cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -p %s" % (proxyIP, util.jsonStr2SshpassArg(self.__jsonStr))
			print cmd
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
			if status != 0:
				errMsg = "Failed to deploy proxy %s for %s" % (proxyIP, stderr)
				logger.error(errMsg)
				self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=errMsg)
				return -4

			logger.info("Succeeded to deploy proxy %s" % proxyIP)
			self.__updateProgress(success=True, ip=proxyIP, swiftType="proxy", msg="")
			return 0

		except util.TimeoutError as err:
			logger.error("%s" % err)
			sys.stderr.write("%s\n" % err)
			self.__updateProgress(success=False, ip=proxyIP, swiftType="proxy", msg=err)

	def proxyDeploy(self):
		logger = util.getLogger(name="proxyDeploy")
		argumentList = []
		for i in [node["ip"] for node in self.__proxyList]:
			argumentList.append(([i], None))
		pool = threadpool.ThreadPool(10)
		requests = threadpool.makeRequests(self.__proxyDeploySubtask, argumentList)
		for req in requests:
			pool.putRequest(req)
		pool.wait()
		pool.dismissWorkers(10)
		pool.joinAllDismissedWorkers()

	#@timeout(self.__kwparams['storageInterval'], -1)
	def __storageDeploySubtask(self, storageIP):
		logger = util.getLogger(name="storageDeploySubtask: %s" % storageIP)
		try:
			if util.spreadMetadata(password=self.__kwparams['password'], sourceDir='/etc/delta/swift', nodeList=[storageIP])[0] != 0:
				errMsg = "Failed to spread metadata to %s" % storageIP
				logger.error(errMsg)
				self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=errMsg)
				return -2

			cmd = "scp -r /DCloudSwift/ root@%s:/" % storageIP
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
			if status != 0:
				errMsg = "Failed to scp storage scripts to %s for %s" % (storageIP, stderr)
				logger.error(errMsg)
				self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=errMsg)
				return -3

			cmd = "ssh root@%s python /DCloudSwift/storage/CmdReceiver.py -s %s"%(storageIP, util.jsonStr2SshpassArg(self.__jsonStr))
			print cmd
			(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
			if status != 0:
				errMsg = "Failed to deploy storage %s for %s" % (storageIP, stderr)
				logger.error(errMsg)
				self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=errMsg)
				return -4

			logger.info("Succeeded to deploy storage %s" % storageIP)
			self.__updateProgress(success=True, ip=storageIP, swiftType="storage", msg="")
			return 0

		except util.TimeoutError as err:
			logger.error("%s" % err)
			sys.stderr.write("%s\n" % err)
			self.__updateProgress(success=False, ip=storageIP, swiftType="storage", msg=err)

	def storageDeploy(self):
		logger = util.getLogger(name="storageDeploy")
		argumentList = []
                for i in [node["ip"] for node in self.__storageList]:
                        argumentList.append(([i], None))
                pool = threadpool.ThreadPool(20)
                requests = threadpool.makeRequests(self.__storageDeploySubtask, argumentList)
                for req in requests:
                        pool.putRequest(req)
                pool.wait()
                pool.dismissWorkers(20)
                pool.joinAllDismissedWorkers()

	def addStorage(self):
		logger = util.getLogger(name="addStorage")
		self.storageDeploy()

		for i in [node["ip"] for node in self.__proxyList]:
			try:
				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
				(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
				if status !=0:
					logger.error("Failed to scp proxy scrips to %s for %s"%(i, stderr))
					continue
			
				#TODO: Monitor Progress report
				cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -a %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd

				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd)
			
				if status != 0:
					logger.error("Failed to addStorage from proxy %s for %s"%(i, stderr))
					continue

				#TODO: Return black list
				return (0,[],[])
			except util.TimeoutError as err:
				logger.error("%s"%err)
                        	sys.stderr.write("%s\n"%err)
		
		logger.error("Failed to addStorage\n")
		return (1, self.__proxyList, self.__storageList)

	def rmStorage(self):
		logger = util.getLogger(name="rmStorage")

		for i in [node["ip"] for node in self.__proxyList]:
			try:
				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
				(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
				if status !=0:
					logger.error("Failed to scp proxy scrips to %s for %s"%(i, stderr))
					continue
			
				#TODO: Monitor Progress report
				cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -r %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd

				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd)
				if status != 0:
					logger.error("Failed to rmStorage from proxy %s for %s"%(i, stderr))
					continue

				#TODO: Return black list
				return (0,[],[])
			except util.TimeoutError as err:
				logger.error("%s"%err)
                        	sys.stderr.write("%s\n"%err)

		logger.error("Failed to rmStorage\n")
		return (1, self.__proxyList, self.__storageList)


if __name__ == '__main__':
	#util.spreadPackages(password="deltacloud", nodeList=["172.16.229.122", "172.16.229.34", "172.16.229.46", "172.16.229.73"])
	#util.spreadRC(password="deltacloud", nodeList=["172.16.229.122"])
	SD = SwiftDeploy([{"ip":"192.168.11.6"},{"ip":"192.168.11.7"}], [{"ip":"192.168.11.7", "zid":1}, {"ip":"192.168.11.8", "zid":2}, {"ip":"192.168.11.9", "zid":3}])
	SD.createMetadata()
	#SD.rmStorage()
	#SD.addStorage()
	SD.proxyDeploy()
	SD.storageDeploy()

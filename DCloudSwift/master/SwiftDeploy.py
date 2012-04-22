import os, sys, socket
import posixfile
import time
import json
import subprocess
import threading
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

		os.system("mkdir -p %s" % self.__kwparams['logDir'])
		os.system("mkdir -p %s" % self.__kwparams['reportDir'])

#		if self.__kwparams['numOfReplica'] > len(self.__storageList):
#			errMsg = "The number of storage nodes is less than the number of replicas!"
#			print "[Error]: %s" % errMsg
#			logging.error(errMsg)
#			sys.exit(1)

		self.__jsonStr = json.dumps(self.__kwparams)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

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

		os.system("/DCloudSwift/proxy/CreateRings.sh %d" % numOfReplica)
		zoneNumber = 1
		for node in storageList: 
			for j in range(deviceCnt):
				deviceName = devicePrx + str(j+1)
				logger.info("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s"% (node["zid"], node["ip"], deviceName))
				os.system("/DCloudSwift/proxy/AddRingDevice.sh %d %s %s" % (node["zid"], node["ip"], deviceName))

		os.system("/DCloudSwift/proxy/Rebalance.sh")

		os.system("cp -r /etc/swift /etc/delta")
		os.system("rm -rf /etc/swift/*")
		os.system("cd /etc/delta/swift; rm -rf %s"%UNNECESSARYFILES)



	def proxyDeploy(self):
		logger = util.getLogger(name="proxyDeploy")
		#TODO: 1) use fork and report progress 2) deploy multiple proxy nodes

		blackList = []
		for i in [node["ip"] for node in self.__proxyList]:
			try:
							
				if util.spreadMetadata(password=self.__kwparams['password'], sourceDir='/etc/delta/swift', nodeList=[i])[0] !=0:
					logger.error("Failed to spread metadata to %s"%i)
					blackList.append(i)
					continue

				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
                        	(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
                        	if status !=0:
                        		logger.error("Failed to scp proxy deploy scrips to %s for %s"%(i, stderr))
					blackList.append(i)
					continue

				cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -p %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
                        	print cmd
                        	(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
                        	if status != 0:
                        		logger.error("Failed to deploy proxy %s for %s"%(i, stderr))
					blackList.append(i)
					continue
			
				logger.info("Succedded to deploy proxy %s"%i)

			except util.TimeoutError as err:
				logger.error("%s"%err)
				sys.stderr.write("%s\n"%err)
				blackList.append(i)

		return (0, blackList)


	def storageDeploy(self):
		logger = util.getLogger(name="storageDeploy")
		blackList =[]
		#TODO: use thread pool and report progress
		for i in [node["ip"] for node in self.__storageList]:
		#	pid = os.fork()
		#	if pid == 0:
		#		continue
			try:
				if util.spreadMetadata(password=self.__kwparams['password'], sourceDir='/etc/delta/swift', nodeList=[i])[0] !=0:
                                        logger.error("Failed to spread metadata to %s"%i)
                                        blackList.append(i)
                                        continue

				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
                                (status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
                                if status !=0:
                                        logger.error("Failed to scp storage scrips to %s for %s"%(i, stderr))
                                        continue

				cmd = "ssh root@%s python /DCloudSwift/storage/CmdReceiver.py -s %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd
				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
				if status != 0:
                                        logger.error("Failed to deploy storage %s for %s"%(i, stderr))
                                        continue

			except util.TimeoutError as err:
				logger.error("%s"%err)
                        	sys.stderr.write("%s\n"%err)

		#	if pid != 0:
		#		os._exit(0)
			
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
	SD = SwiftDeploy([{"ip":"192.168.11.6"}], [{"ip":"192.168.11.7", "zid":1}, {"ip":"192.168.11.8", "zid":2}])
	SD.createMetadata()
	#SD.rmStorage()
	#SD.addStorage()
	SD.proxyDeploy()
	#SD.storageDeploy()

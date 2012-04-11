'''
Created on 2012/03/01

@author: CW

Modified by CW on 2012/03/02
Modified by CW on 2012/03/03
Modified by CW on 2012/03/05
Modified by CW on 2012/03/06
Modified by CW on 2012/03/07
Modified by Ken on 2012/03/09
Modified by Ken on 2012/03/12
Modified by Ken on 2012/03/13
Modified by Ken on 2012/03/15
Modified by Ken on 2012/03/16
Modified by Ken on 2012/03/17
Modified by CW on 2012/03/22: correct the absolute path of function proxyDeploy()
Modified by Ken on 2012/03/28: pass argument by appending json string to the end of sshpass command 
'''

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import threading
import datetime
import logging
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

#Self defined packages
sys.path.append("/DCloudSwift/util")
from SwiftCfg import SwiftCfg
import util



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

		if not util.isAllDebInstalled("/DCloudSwift/master/deb_source/"):
			util.installAllDeb("/DCloudSwift/master/deb_source/")

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def __firstProxyDeploy(self):
		logger = util.getLogger(name="firstProxyDeploy")
                #TODO: 1) use fork and report progress 2) deploy multiple proxy nodes
                i = self.__proxyList[0]
                try:

                        cmd = "scp -r /DCloudSwift/ root@%s:/"%i
                        (status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
                        if status !=0:
                                logger.error("Failed to scp proxy deploy scrips to %s for %s"%(i, stderr.read()))
                                return 1

                        cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -f %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
                        print cmd
                        (status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
                        if status != 0:
                                logger.error("Failed to deploy the first proxy %s for %s"%(i, stderr.read()))
                                return 1

			cmd = "scp -r root@%s:/etc/swift /tmp"%i
			(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
                        if status !=0:
                                logger.error("Failed to retrieve /etc/swift from %s for %s"%(i, stderr.read()))
                                return 1

			return 0

                except util.TimeoutError as err:
                        logger.error("%s"%err)
                        sys.stderr.write("%s\n"%err)
			return 1
			


	def proxyDeploy(self):
		logger = util.getLogger(name="proxyDeploy")
		#TODO: 1) use fork and report progress 2) deploy multiple proxy nodes
		if self.__firstProxyDeploy() !=0:
			return (1, self.__proxyList)

		blackList = []
		for i in self.__proxyList[1:]:
			try:
							
				if util.spreadMetadata(password=self.__kwparams['password'], sourceDir='/tmp/swift', nodeList=[i])[0] !=0:
					logger.error("Failed to spread metadata to %s"%i)
					blackList.append(i)
					continue

				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
                        	(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
                        	if status !=0:
                        		logger.error("Failed to scp proxy deploy scrips to %s for %s"%(i, stderr.readlines()))
					blackList.append(i)
					continue

				cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -p %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
                        	print cmd
                        	(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
                        	if status != 0:
                        		logger.error("Failed to deploy proxy %s for %s"%(i, stderr.readlines()))
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
		#TODO: use thread pool and report progress
		for i in self.__storageList:
		#	pid = os.fork()
		#	if pid == 0:
		#		continue
			try:
				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
                                (status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
                                if status !=0:
                                        logger.error("Failed to scp storage scrips to %s for %s"%(i, stderr.readlines()))
                                        continue

				cmd = "ssh root@%s python /DCloudSwift/storage/CmdReceiver.py -s %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd
				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd, timeout=360)
				if status != 0:
                                        logger.error("Failed to deploy storage %s for %s"%(i, stderr.readlines()))
                                        continue

			except util.TimeoutError as err:
				logger.error("%s"%err)
                        	sys.stderr.write("%s\n"%err)

		#	if pid != 0:
		#		os._exit(0)
			
	def addStorage(self):
		logger = util.getLogger(name="addStorage")
		self.storageDeploy()

		for i in self.__proxyList:
			try:
				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
				(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
				if status !=0:
					logger.error("Failed to scp proxy scrips to %s for %s"%(i, stderr.readlines()))
					continue
			
				#TODO: Monitor Progress report
				cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -a %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd

				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd)
			
				if status != 0:
					logger.error("Failed to addStorage from proxy %s for %s"%(i, stderr.readlines()))
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

		for i in self.__proxyList:
			try:
				cmd = "scp -r /DCloudSwift/ root@%s:/"%i
				(status, stdout, stderr) = util.sshpass(self.__kwparams['password'], cmd, timeout=60)
				if status !=0:
					logger.error("Failed to scp proxy scrips to %s for %s"%(i, stderr.readlines()))
					continue
			
				#TODO: Monitor Progress report
				cmd = "ssh root@%s python /DCloudSwift/proxy/CmdReceiver.py -r %s"%(i, util.jsonStr2SshpassArg(self.__jsonStr))
				print cmd

				(status, stdout, stderr)  = util.sshpass(self.__kwparams['password'], cmd)
			
				if status != 0:
					logger.error("Failed to rmStorage from proxy %s for %s"%(i, stderr.readlines()))
					continue

				#TODO: Return black list
				return (0,[],[])
			except util.TimeoutError as err:
				logger.error("%s"%err)
                        	sys.stderr.write("%s\n"%err)

		logger.error("Failed to rmStorage\n")
		return (1, self.__proxyList, self.__storageList)


if __name__ == '__main__':
	SD = SwiftDeploy(['172.16.229.34','172.16.229.73'], ['172.16.229.46', '172.16.229.73'])
	#SD.rmStorage()
	#SD.addStorage()
	SD.proxyDeploy()
	#TODO: maybe need some time to wait for proxy deploy
	#SD.storageDeploy()

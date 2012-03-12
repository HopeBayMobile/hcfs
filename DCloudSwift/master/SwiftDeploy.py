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
'''

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import datetime
import logging
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

#Self defined packages
sys.path.append("../util")
from SwiftCfg import SwiftCfg
import util


class SwiftDeploy:
	def __init__(self, configFile, proxyList = [], storageList = []):
		self.__configFile = configFile
		self.__proxyList = proxyList
		self.__storageList = storageList

		self.__SC = SwiftCfg(self.__configFile)
		self.__kwparams = self.__SC.getKwparams()
		self.__kwparams['proxyList'] = self.__proxyList
		self.__kwparams['storageList'] = self.__storageList

		os.system("mkdir -p %s" % self.__kwparams['logDir'])
		os.system("mkdir -p %s" % self.__kwparams['reportDir'])

#		if self.__kwparams['numOfReplica'] > len(self.__storageList):
#			errMsg = "The number of storage nodes is less than the number of replicas!"
#			print "[Error]: %s" % errMsg
#			logging.error(errMsg)
#			sys.exit(1)

		self.__jsonStr = json.dumps(self.__kwparams)

		os.system("dpkg -i ./debsrc/*.deb")
		os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")


	def proxyDeploy(self):
		#TODO: use fork and report progress
		for i in self.__proxyList:
			scpStatus = os.system("sshpass -p %s scp -r ../proxy root@%s:/" % (self.__kwparams['password'], i))
			if scpStatus != 0:
				errMsg = "Fail to scp the proxy node: " + i
				print "[Debug]: %s" % errMsg
				logging.debug(errMsg)
				sys.exit(1)

			os.system("echo \'%s\' > ProxyParams" % self.__jsonStr)
			os.system("sshpass -p %s scp ProxyParams root@%s:/proxy" % (self.__kwparams['password'], i))

			cmd = "python /proxy/CmdReceiver.py -p"
			sshpassStatus = os.system("sshpass -p %s ssh root@%s %s > %s/proxyDeploy_%s.log"\
					 % (self.__kwparams['password'], i, cmd, self.__kwparams['logDir'], i))
			if sshpassStatus != 0:
				errMsg = "Fail to deploy the proxy node: " + i
				print "[Debug]: %s" % errMsg
				logging.debug(errMsg)
				sys.exit(1)


	def storageDeploy(self):
		#TODO: use fork and report progress
		for i in self.__storageList:
			pid = os.fork()
			if pid == 0:
				continue

			ScpStatus = os.system("sshpass -p %s scp -r ../storage root@%s:/" % (self.__kwparams['password'], i))
			if ScpStatus != 0:
				errMsg = "Fail to scp the storage node: " + i
				print "[Debug]: %s" % errMsg
				logging.debug(errMsg)
				sys.exit(1)

			os.system("echo \'%s\' > StorageParams" % self.__jsonStr)
                        os.system("sshpass -p %s scp StorageParams root@%s:/storage" % (self.__kwparams['password'], i))

			cmd = "python /storage/CmdReceiver.py -s"
			sshpassStatus = os.system("sshpass -p %s ssh root@%s %s > %s/storageDeploy_%s.log"\
					 % (self.__kwparams['password'], i, cmd, self.__kwparams['logDir'], i))
			if sshpassStatus != 0:
				errMsg = "Fail to deploy the storage node: " + i
				print "[Debug]: %s" % errMsg
				logging.debug(errMsg)
				sys.exit(1)

			if pid != 0:
				os._exit(0)
			
	def addStorage(self):
		logger = util.getLogger(name="addStorage")
		self.storageDeploy()

		for i in self.__proxyList:
			try:
				#TODO: read timeout setting from configure files
				cmd = "scp -r ../proxy root@%s:/"%i
				(status, stdout) = util.sshpass(self.__kwparams['password'], cmd, timeout=20)
				if status !=0:
					logger.error("Failed to scp proxy scrips to %s for %s"%(i, stdout.readlines()))
					continue
			
				#TODO: directly send params
				os.system("echo \'%s\' > AddStorageParams" % self.__jsonStr)
				cmd = "scp AddStorageParams root@%s:/proxy" % i
				(status, stdout) = util.sshpass(self.__kwparams['password'], cmd, timeout=20)

				if status !=0:
					logger.error("Failed to scp params to %s for %s"%(i, stdout.readlines()))
					continue

				#TODO: Monitor Progress report
				cmd = "ssh root@%s python /proxy/CmdReceiver.py -a"%i
				(status, stdout)  = util.sshpass(self.__kwparams['password'], cmd)
			
				if status != 0:
					logger.error("Failed to addStorage from proxy %s for %s"%(i, stdout.readlines()))
					continue

				#TODO: Return black list
			except TimeoutError as err:
				print err
		
		logger.error("Failed to addStorage\n")
		return self.__storageList

if __name__ == '__main__':
	SD= SwiftDeploy("../Swift.ini", ['192.168.1.81'], ['192.168.1.82', '192.168.1.83'])
	SD.addStorage()
	#SD.proxyDeploy()
	#TODO: maybe need some time to wait for proxy deploy
	#SD.storageDeploy()


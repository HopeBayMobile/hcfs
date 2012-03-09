'''
Created on 2012/03/01

@author: CW

Modified by CW on 2012/03/02
Modified by CW on 2012/03/03
Modified by CW on 2012/03/05
Modified by CW on 2012/03/06
Modified by CW on 2012/03/07
Modified by Ken on 2012/03/09
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

		if self.__kwparams['numOfReplica'] > len(self.__storageList):
			errMsg = "The number of storage nodes is less than the number of replicas!"
			print "[Error]: %s" % errMsg
			logging.error(errMsg)
			sys.exit(1)

		self.__jsonStr = json.dumps(self.__kwparams)

		os.system("dpkg -i ./sshpass_1.05-1_amd64.deb")
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
			

if __name__ == '__main__':
	SD= SwiftDeploy("../Swift.ini", ['192.168.122.183'], ['192.168.122.139', '192.168.122.168'])
	SD.proxyDeploy()
	#TODO: maybe need some time to wait for proxy deploy
	SD.storageDeploy()


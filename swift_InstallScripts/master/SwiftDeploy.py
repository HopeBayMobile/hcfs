'''
Created on 2012/03/01

@author: CW

Modified by CW on 2012/03/02
Modified by CW on 2012/03/03
'''

import sys
import os
import socket
import posixfile
import time
import json
import subprocess
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

#Self defined packages
from SwiftCfg import SwiftCfg


class SwiftDeploy:
	def __init__(self, configFile, proxyList = [], storageList = []):
		self.__configFile = configFile
		self.__proxyList = proxyList
		self.__storageList = storageList

		self.__SC = SwiftCfg(self.__configFile)
		self.__username = self.__SC.getUsername()
		self.__password = self.__SC.getPassword()
		self.__logDir = self.__SC.getLogDir()
		self.__reportDir = self.__SC.getReportDir()
		self.__proxyInterval = self.__SC.getProxyInterval()
		self.__storageInterval = self.__SC.getStorageInterval()
		self.__numOfReplica = self.__SC.getNumOfReplica()
		self.__deviceName = self.__SC.getDeviceName()

		os.system("mkdir -p %s" % self.__logDir)
		os.system("mkdir -p %s" % self.__reportDir)

		self.__kwparams = {
			'username': self.__username,
			'password': self.__password,
			'proxyList': self.__proxyList,
			'storageList': self.__storageList,
			'logDir': self.__logDir,
			'reportDir': self.__reportDir,
			'proxyInterval': self.__proxyInterval,
			'storageInterval': self.__storageInterval,
			'numOfReplica': self.__numOfReplica,
			'deviceName': self.__deviceName
		}
		self.__jsonStr = json.dumps(self.__kwparams)
		#TODO: check the error of the configuration file and the number of replicas

		os.system("dpkg -i ./sshpass_1.05-1_amd64.deb")
		os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def proxyDeploy(self):
		#TODO: use fork and report progress
		for i in self.__proxyList:
			scpStatus = os.system("sshpass -p %s scp -r ../proxy root@%s:/" % (self.__password, i))
			if scpStatus != 0:
				print "Fail to scp the proxy node: %s" % i
				sys.exit(1)
			os.system("echo \'%s\' > ProxyParams" % self.__jsonStr)
			os.system("sshpass -p %s scp ProxyParams root@%s:/proxy" % (self.__password, i))
			cmd = "python /proxy/CmdReceiver.py -p"
			sshpassStatus = os.system("sshpass -p %s ssh root@%s %s > %s/proxyDeploy_%s.log"\
					 % (self.__password, i, cmd, self.__logDir, i))
			if sshpassStatus != 0:
				print "Fail to deploy the proxy node: %s" % i
				sys.exit(1)

	def storageDeploy(self):
		#TODO: use fork and report progress
		for i in self.__storageList:
			ScpStatus = os.system("sshpass -p %s scp -r ../storage root@%s:/" % (self.__password, i))
			if ScpStatus != 0:
				print "Fail to scp the storage node: %s" % i
				sys.exit(1)
			os.system("echo \'%s\' > StorageParams" % self.__jsonStr)
                        os.system("sshpass -p %s scp StorageParams root@%s:/storage" % (self.__password, i))
			cmd = "python /storage/CmdReceiver.py -s"
			sshpassStatus = os.system("sshpass -p %s ssh root@%s %s > %s/storageDeploy_%s.log"\
					 % (self.__password, i, cmd, self.__logDir, i))
			if sshpassStatus != 0:
				print "Fail to deploy the storage node: %s" % i
				sys.exit(1)
			


if __name__ == '__main__':
	SD= SwiftDeploy("./Swift.ini", ['192.168.122.183'], ['192.168.122.139', '192.168.122.168'])
	SD.proxyDeploy()
	#TODO: maybe need some time to wait for proxy deploy
	SD.storageDeploy()


'''
Created on 2012/03/01

@author: CW

Modified by CW on 2012/03/06
Modified by CW on 2012/03/07
Modified by Ken on 2012/03/09
'''

import sys
import os
import socket
import posixfile
import json
import datetime
import logging
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser


class SwiftCfg:
	def __init__(self, configFile):
		self.__kwparams = {}
		self.__configFile = configFile

		config = ConfigParser()
		config.readfp(open(self.__configFile))

		self.__logDir = config.get('log', 'dir')
		self.__logName = config.get('log', 'name')
		self.__logLevel = config.get('log', 'level')
		self.__reportDir = config.get('report', 'dir')

		self.__username = config.get('main', 'username')
		self.__password = config.get('main', 'password')

		self.__proxyInterval = config.get('timeout', 'proxyInterval')
		self.__proxyInterval = int(self.__proxyInterval)
		self.__storageInterval = config.get('timeout', 'storageInterval')
		self.__storageInterval = int(self.__storageInterval)

		self.__numOfReplica = config.get('proxy', 'replica')
		self.__numOfReplica = int(self.__numOfReplica)
		self.__proxyInterface = config.get('proxy', 'proxyInterface')

		self.__deviceName = config.get('storage', 'device')
		self.__storageInterface = config.get('storage', 'storageInterface')

		self.__kwparams = {
			'logDir': self.__logDir,
			'logLevel':self.__logLevel,
			'logName': self.__logName,
			'reportDir': self.__reportDir,
			'username': self.__username,
			'password': self.__password,
			'proxyInterval': self.__proxyInterval,
			'storageInterval': self.__storageInterval,
			'numOfReplica': self.__numOfReplica,
			'proxyInterface': self.__proxyInterface,
			'deviceName': self.__deviceName,
			'storageInterface': self.__storageInterface
		}

		os.system("mkdir -p "+self.__kwparams['logDir'])
		os.system("touch "+ self.__kwparams['logDir'] + self.__kwparams['logName'])
		logging.basicConfig(level = logging.DEBUG,
			format = '[%(levelname)s on %(asctime)s] %(message)s',
			filename = self.__kwparams['logDir'] + self.__kwparams['logName']
		)

		infoMsg = "The parsing of Swift configuration has been finished!"
		logging.info(infoMsg)

	def getKwparams(self):
		return self.__kwparams
	

if __name__ == '__main__':
	SC = SwiftCfg("/etc/deltaSwift/Swift.ini")
	kwparams = SC.getKwparams()
	print "Username: %s, Password: %s" % (kwparams['username'], kwparams['password'])
	print "logDir: %s, reportDir: %s" % (kwparams['logDir'], kwparams['reportDir'])
	print "proxyinterval: %d, storageinterval: %d" % (kwparams['proxyInterval'], kwparams['storageInterval'])
	print "numofreplica: %d, nameofdevice: %s" % (kwparams['numOfReplica'], kwparams['deviceName'])

'''
Created on 2012/03/01

@author: CW
'''

import sys
import os
import socket
import posixfile
import json
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser


class SwiftCfg:
	def __init__(self, configFile):
		self.__configFile = configFile

		config = ConfigParser()
		config.readfp(open(self.__configFile))

		self.__logDir = config.get('log', 'dir')
		self.__reportDir = config.get('report', 'dir')

		self.__username = config.get('main', 'username')
		self.__password = config.get('main', 'password')

		self.__proxyInterval = config.get('timeout', 'proxyInterval')
		self.__storageInterval = config.get('timeout', 'storageInterval')

		self.__numOfReplica = config.get('proxy', 'replica')
		self.__deviceName = config.get('storage', 'device')

	def getUsername(self):
		return self.__username

	def getPassword(self):
		return self.__password

	def getLogDir(self):
		return self.__logDir

	def getReportDir(self):
		return self.__reportDir

	def getProxyInterval(self):
		return int(self.__proxyInterval)

	def getStorageInterval(self):
		return int(self.__storageInterval)

	def getNumOfReplica(self):
		return int(self.__numOfReplica)

	def getDeviceName(self):
		return self.__deviceName
	

if __name__ == '__main__':
	SC = SwiftCfg("./Swift.ini")
	print "Username: %s, Password: %s" % (SC.getUsername(), SC.getPassword())
	print "logDir: %s, reportDir: %s" % (SC.getLogDir(), SC.getReportDir())
	print "proxyinterval: %d, storageinterval: %d" % (SC.getProxyInterval(), SC.getStorageInterval())
	print "numofreplica: %d, nameofdevice: %s" % (SC.getNumOfReplica(), SC.getDeviceName())

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
import socket

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/util"%BASEDIR)

from util import util
from util import mountDisks

class StorageNodeInstaller:
	def __init__(self, proxy, proxyList, devicePrx="sdb", deviceCnt=1):
		self.__proxy = proxy
		self.__proxyList = proxyList
		self.__devicePrx = devicePrx
		self.__deviceCnt = deviceCnt
		self.__privateIP = socket.gethostbyname(socket.gethostname())
		
		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

		os.system("mkdir -p /etc/swift")
		os.system("chown -R swift:swift /etc/swift/")
		os.system("mkdir -p /srv/node/")

		self.__logger = util.getLogger(name = "StorageNodeInstaller")

	def install(self):
		self.__logger.info("start install")

		util.stopAllServices()

		metadata = mountDisks.getLatestMetadata()
		if metadata is None or metadata["vers"] < util.getSwiftConfVers():
			mountDisks.createSwiftDevices(deviceCnt=self.__deviceCnt,devicePrx=self.__devicePrx)
			if BASEDIR != "/":
				os.system("rm -rf /DCloudSwift")
				os.system("cp -r %s/DCloudSwift /"%BASEDIR)
		else:
			mountDisks.remountDisks()

		os.system("chown -R swift:swift /srv/node/ ")
		util.generateSwiftConfig()

		#TODO:check if this node is a proxy node
		util.restartAllServices()
		
		self.__logger.info("end install")

if __name__ == '__main__':
	pass

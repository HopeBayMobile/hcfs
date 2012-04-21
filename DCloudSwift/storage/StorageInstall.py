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
sys.path.append("/DCloudSwift/util")
from SwiftCfg import SwiftCfg
import util
import mountDisks

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
		metadata = mountDisks.getLatestMetadata()
		if metadata is None or metadata["vers"] < util.getSwiftConfVers():
			mountDisks.createSwiftDevices(deviceCnt=self.__deviceCnt,devicePrx=self.__devicePrx)
		else:
			mountDisks.remountDisks()

		os.system("chown -R swift:swift /srv/node/ ")
		util.generateSwiftConfig()

		if util.restartRsync() !=0:
			self.__logger.error("Failed to restart rsync daemon")

		#os.system("perl -pi -e \'s/MAX_META_VALUE_LENGTH = 256/MAX_META_VALUE_LENGTH = 512/\' /usr/share/pyshared/swift/common/constraints.py")
		#TODO:check if this node is a proxy node
		os.system("swift-init all restart")
		
		self.__logger.info("end install")

if __name__ == '__main__':
	pass

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
import mountDisks

class StorageNodeInstaller:
	def __init__(self, proxy, proxyList, devicePrx="sdb", deviceCnt=1):
		self.__proxy = proxy
		self.__proxyList = proxyList
		self.__devicePrx = devicePrx
		self.__deviceCnt = deviceCnt
		
		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

		os.system("mkdir -p /etc/swift")
		os.system("chown -R swift:swift /etc/swift/")
		os.system("mkdir -p /srv/node/")

		self.__logger = util.getLogger(name = "StorageNodeInstaller")

	def install(self):
		self.__logger.info("getlatestMetadata")
		metadata = mountDisks.getLatestMetadata()
		if metadata is None:
			self.__logger.info("Write Metadata")
			mountDisks.createSwiftDevices(proxyList=self.__proxyList, deviceCnt=self.__deviceCnt,devicePrx=self.__devicePrx)

		os.system("chown -R swift:swift /srv/node/ ")
		os.system("/DCloudSwift/storage/rsync.sh")
		os.system("perl -pi -e 's/RSYNC_ENABLE=false/RSYNC_ENABLE=true/' /etc/default/rsync")
		os.system("service rsync start")

		os.system("/DCloudSwift/storage/accountserver.sh")
		os.system("/DCloudSwift/storage/containerserver.sh")
		os.system("/DCloudSwift/storage/objectserver.sh")
		os.system("perl -pi -e \'s/MAX_META_VALUE_LENGTH = 256/MAX_META_VALUE_LENGTH = 512/\' /usr/share/pyshared/swift/common/constraints.py")
		os.system("swift-init all start")
		
		#TODO: for NTU mode only
		line1 = " #!/bin/sh -e"
		line2 = "python /DCloudSwift/util/mountDisks.py -r"
		line3 = "python /DCloudSwift/util/mountDisks.py -l"
		os.system("echo \"%s\" > /etc/rc.local"%line1)
		os.system("echo \"%s\" >> /etc/rc.local"%line2)
		os.system("echo \"%s\" >> /etc/rc.local"%line3)

if __name__ == '__main__':
	pass

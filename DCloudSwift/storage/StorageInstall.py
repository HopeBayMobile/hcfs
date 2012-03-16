# This script is used to deploy the storage node of Swift.
# History:
# 2012/03/16 first release by Ken

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
	def __init__(self, proxy, devPrx="sdb", devCnt=1):
		self.__proxy = proxy
		self.__devicePrx = devPrx
		self.__deviceCnt = devCnt
		
		if not util.isAllDebInstalled("/DCloudSwift/storage/deb_source/"):
			util.installAllDeb("/DCloudSwift/storage/deb_source/")
		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

		os.system("mkdir -p /etc/swift")
		os.system("chown -R swift:swift /etc/swift/")
		os.system("mkdir -p /srv/node/")

	def install(self):
		
		cmd = "sshpass -p deltacloud scp root@%s:/etc/swift/swift.conf /etc/swift/"%self.__proxy
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()
		if po.returncode !=0:
			logger.error("Failed to execute %s for %s"%(cmd, po.stderr.readlines()))
			return 1

		cmd = "sshpass -p deltacloud scp root@%s:/etc/swift/*.ring.gz /etc/swift/"%self.__proxy
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()
		if po.returncode !=0:
                	logger.error("Failed to execute %s for %s"%(cmd, po.stderr.readlines()))
                        return 1

		mountDisks.prepareMountPoints(self.__deviceCnt)

		os.system("chown -R swift:swift /srv/node/ ")
		os.system("/DCloudSwift/storage/rsync.sh")
		os.system("perl -pi -e 's/RSYNC_ENABLE=false/RSYNC_ENABLE=true/' /etc/default/rsync")
		os.system("service rsync start")

		os.system("/DCloudSwift/storage/accountserver.sh")
		os.system("/DCloudSwift/storage/containerserver.sh")
		os.system("/DCloudSwift/storage/objectserver.sh")
		os.system("swift-init all start")

if __name__ == '__main__':
	pass

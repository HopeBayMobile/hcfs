# This script is used to deploy the storage node of Swift.
# History:
# 2012/03/16 first release by Ken
# 2012/03/17 modified by Ken
# 2012/03/22 modified by CW: modify MAX_META_VALUE_LENGTH from 256 to 512
# 2012/03/26 modified by CW: correct the declaration of logger 
# 2012/03/28 modified by Ken: overwrite rc.local to remount disks in order

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

		self.__logger = util.getLogger(name = "StorageNodeInstaller")

	def install(self):
		
		cmd = "scp root@%s:/etc/swift/swift.conf /etc/swift/"%self.__proxy
		(ret, stdout, stderr) = util.sshpass(passwd='deltacloud', cmd=cmd, timeout=60) 
		if ret !=0:
			self.__logger.error("Failed to execute %s for %s"%(cmd, stderr.readlines()))
			return 1

		cmd = "scp root@%s:/etc/swift/*.ring.gz /etc/swift/"%self.__proxy
		(ret, stdout, stderr) = util.sshpass(passwd='deltacloud', cmd=cmd, timeout=60) 
		if ret !=0:
			self.__logger.error("Failed to execute %s for %s"%(cmd, stderr.readlines()))
			return 1

		mountDisks.createSwiftDevices(deviceCnt=self.__deviceCnt,devicePrx=self.__devicePrx)

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
		line2 = "python /DCloudSwift/storage/mountDisks.py -r"
		os.system("echo \"%s\" > /etc/rc.local"%line1)
		os.system("echo \"%s\" >> /etc/rc.local"%line2)

if __name__ == '__main__':
	pass

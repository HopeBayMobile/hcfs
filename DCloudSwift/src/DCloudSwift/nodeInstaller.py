import sys
import os
import socket
import time
import json
import subprocess
import random
from ConfigParser import ConfigParser

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(WORKING_DIR)
#os.chdir(WORKING_DIR)
#sys.path.append("%s/DCloudSwift/"%BASEDIR)

from util import util
from util import diskUtil
import maintenance


class NodeInstaller:
    def __init__(self, devicePrx="sdb", deviceCnt=1):
        self.__devicePrx = devicePrx
        self.__deviceCnt = deviceCnt
        self.__privateIP = socket.gethostbyname(socket.gethostname())
        
        util.createRamdiskDirs()

        if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
            os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

        if util.isDaemonAlive("swiftMonitor"):
            os.system("python /DCloudSwift/monitor/swiftMonitor.py stop")

        util.stopAllServices()

        os.system("rm -rf /etc/swift")
        os.system("cp -r %s/swift /etc/" % BASEDIR)
        os.system("chown -R swift:swift /etc/swift")

        self.__logger = util.getLogger(name="nodeInstaller")

    def install(self):
        self.__logger.info("start install")

        fingerprint = diskUtil.getLatestFingerprint()
        if not fingerprint or fingerprint["vers"] < util.getSwiftConfVers():
            if self.__deviceCnt:
                diskUtil.createSwiftDevices(deviceCnt=self.__deviceCnt, devicePrx=self.__devicePrx)
            if BASEDIR != "/":
                os.system("rm -rf /DCloudSwift")
                os.system("cp -r %s/DCloudSwift /" % BASEDIR)
        else:
            diskUtil.remountDisks()

        os.system("chown -R swift:swift /srv/node/ ")
        util.generateSwiftConfig()

        util.restartAllServices()
        os.system("python /DCloudSwift/monitor/swiftMonitor.py restart")

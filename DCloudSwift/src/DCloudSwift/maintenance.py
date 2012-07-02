import os
import sys
import subprocess
import fcntl
import threading
import random
import signal
import socket
import struct
import math
import pickle
import time

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(WORKING_DIR)

from util import util
from util import diskUtil


class UpdateMetadataError(Exception):
    pass


def isNewer(confDir):
    logger = util.getLogger(name="isNewer")
    oriVers = util.getSwiftConfVers(confDir="/etc/swift/")
    vers = util.getSwiftConfVers(confDir=confDir)

    if vers <= oriVers:
        return False

    return True


def updateMetadata(confDir):
    logger = util.getLogger(name="updateMetadata")
    logger.info("updateMetadata start")
    try:

        os.system("mkdir -p /etc/swift")
        os.system("cp -r %s/* /etc/swift/" % confDir)
        if BASEDIR != "/":
            os.system("python /DCloudSwift/monitor/swiftMonitor.py stop")
            os.system("rm -rf /DCloudSwift")
            os.system("cp -r %s/DCloudSwift /" % BASEDIR)
            os.system("chown -R swift:swift /etc/swift")

        deviceCnt = util.getDeviceCnt()
        devicePrx = util.getDevicePrx()

        oriVers = diskUtil.getLatestVers()
        if oriVers is None:
            util.stopAllServices()  # prevent services from occupying disks
            diskUtil.createSwiftDevices(deviceCnt=deviceCnt, devicePrx=devicePrx)
            util.restartAllServices()
        else:
            diskUtil.updateMetadataOnDisks(oriVers=oriVers)
            diskUtil.mountUmountedSwiftDevices()

        util.startAllServices()
        os.system("python /DCloudSwift/monitor/swiftMonitor.py restart")

    except Exception:
        raise
    finally:
        logger.info("updateMetadata end")


def resume():
    logger = util.getLogger(name="resume")
    logger.info("start")

    os.system("python /DCloudSwift/monitor/swiftMonitor.py stop")
    util.stopAllServices()

    diskUtil.remountDisks()
    diskUtil.loadSwiftMetadata()

    util.restartAllServices()
    os.system("python /DCloudSwift/monitor/swiftMonitor.py restart")

    logger.info("end")


def main(argv):
    ret = 0
    if len(argv) > 0:
        if sys.argv[1] == "-R":
            resume()
        else:
            sys.exit(-1)
    return ret

if __name__ == '__main__':
    main(sys.argv[1:])

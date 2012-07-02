import sys
import os
import socket
import posixfile
import time
import json
import subprocess
import shlex
import random
import fcntl
import pickle
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(WORKING_DIR)

import maintenance
from util import util
from util import diskUtil
from nodeInstaller import NodeInstaller

Usage = '''
Usage:
    python CmdReceiver.py [Option] [jsonStr]
Options:
    [-p | proxy] - for deploying proxy node
    [-u | updateMetadata] - for updating metadata
    [-s | storage] - for deploying storage node
    [-c | cleanMetadata] - for cleaning metadata
Examples:
    python CmdReceiver.py -p {"password": "deltacloud"}
'''


class UsageError(Exception):
    pass


def usage():
    print >> sys.stderr, Usage


def triggerUpdateMetadata(confDir):
    logger = util.getLogger(name="triggerUpdateMetadata")
    logger.info("start")

    try:
        if not maintenance.isNewer(confDir=confDir):
            logger.info("Already the latest metadata")

        else:
            versOnDisks = diskUtil.getLatestVers()
            newVers = util.getSwiftConfVers(confDir=confDir)

            if versOnDisks is not None and versOnDisks > newVers and diskUtil.loadScripts() == 0:
                logger.info("Resume servcies from metadata on disks")
                diskUtil.resume()
            else:
                maintenance.updateMetadata(confDir=confDir)
                if not util.isDaemonAlive("swiftMonitor"):
                    os.system("python /DCloudSwift/monitor/swiftMonitor.py restart")

    except maintenance.UpdateMedatataError:
        raise
    except Exception as e:
        msg = "Failed update Metadata for unexpected exception %s" % str(e)
        logger.error(msg)
        raise
    finally:
        logger.info("end")


def triggerCleanMetadata():
    logger = util.getLogger(name="triggerUpdateMetadata")
    logger.info("start")

    try:
        os.system("python /DCloudSwift/monitor/swiftMonitor.py stop")
        util.stopAllServices()
        if diskUtil.cleanMetadataOnDisks() != 0:
            raise Exception("errors occurred in cleanMetadataOnDisks")
        os.system("rm -rf /etc/swift")

    except Exception as e:
        msg = "Failed to clean metadata for %s" % str(e)
        logger.error(msg)
        raise
    finally:
        logger.info("end")


def triggerProxyDeploy(**kwargs):
    logger = util.getLogger(name="triggerProxyDeploy")
    logger.info("start")

    deviceCnt = kwargs['deviceCnt']
    devicePrx = kwargs['devicePrx']

    installer = NodeInstaller(devicePrx=devicePrx, deviceCnt=deviceCnt)
    installer.install()

    logger.info("end")
    return 0


def triggerStorageDeploy(**kwargs):
    logger = util.getLogger(name="triggerStorageDeploy")
    logger.info("start")

    devicePrx = kwargs['devicePrx']
    deviceCnt = kwargs['deviceCnt']

    installer = NodeInstaller(devicePrx=devicePrx, deviceCnt=deviceCnt)
    installer.install()

    logger.info("end")
    return 0


@util.tryLock()
def main():
    returncode = 0

    try:

        if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
            os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

        if (len(sys.argv) == 3):
            kwargs = None
            if (sys.argv[1] == 'proxy' or sys.argv[1] == '-p'):
                kwargs = json.loads(sys.argv[2])
                print 'Proxy deployment start'
                triggerProxyDeploy(**kwargs)
            elif (sys.argv[1] == 'storage' or sys.argv[1] == '-s'):
                kwargs = json.loads(sys.argv[2])
                print 'storage deployment start'
                triggerStorageDeploy(**kwargs)
            elif (sys.argv[1] == 'updateMetadata' or sys.argv[1] == '-u'):
                print 'updateMetadata start'
                confDir = sys.argv[2]
                triggerUpdateMetadata(confDir=confDir)
            elif (sys.argv[1] == 'cleanMetadata' or sys.argv[1] == '-c'):
                print 'cleanMetadata start'
                triggerCleanMetadata()
            else:
                print >> sys.stderr, "Usage error: Invalid optins"
                raise UsageError
        else:
            raise UsageError
    except UsageError:
        usage()
        returncode = 1
    except ValueError:
        print >>sys.stderr, "Usage error: Ivalid json format"
        returncode = 1
    except Exception as e:
        print >>sys.stderr, str(e)
        returncode = 1
    finally:
        return returncode

if __name__ == '__main__':
    retcode = 0
    try:
        retcode = main()
    except util.TryLockError as e:
        print >>sys.stderr, str(e)
        retcode = 1

    sys.exit(retcode)

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


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))


class SwiftCfg:
    def __init__(self, configFile):
        self.__kwparams = {}
        self.__configFile = configFile

        config = ConfigParser()
        config.readfp(open(self.__configFile))

        self.__logDir = config.get('log', 'dir')
        self.__logName = config.get('log', 'name')
        self.__logLevel = config.get('log', 'level')

        self.__username = "root"

        self.__password = config.get('storage', 'password')
        self.__deviceCnt = int(config.get('storage', 'deviceCnt'))
        self.__devicePrx = "sdb"
        self.__proxyPort = int(config.get('storage', 'proxyPort'))

        self.__portalUrl = config.get('portal', 'url')

        self.__kwparams = {
            'logDir': self.__logDir,
            'logLevel': self.__logLevel,
            'logName': self.__logName,
            'username': self.__username,
            'password': self.__password,
            'devicePrx': self.__devicePrx,
            'deviceCnt': self.__deviceCnt,
            'proxyPort': self.__proxyPort,
            'portalUrl': self.__portalUrl
        }

        os.system("mkdir -p " + self.__kwparams['logDir'])
        os.system("touch " + self.__kwparams['logDir'] + "/" + self.__kwparams['logName'])
        logging.basicConfig(level=logging.DEBUG,
            format='[%(levelname)s on %(asctime)s] %(message)s',
            filename=self.__kwparams['logDir'] + self.__kwparams['logName']
        )

    def getKwparams(self):
        return self.__kwparams


class SwiftMasterCfg:
    def __init__(self, configFile):
        self.__kwparams = {}
        self.__configFile = configFile

        config = ConfigParser()
        config.readfp(open(self.__configFile))

        self.__eventMgrPort = config.get('eventMgr', 'port')
        self.__eventMgrPage = config.get('eventMgr', 'page')
        self.__maintainCheckerReplTime = config.get('maintain_checker', 'replication_time')
        self.__maintainCheckerRefreshTime = config.get('maintain_checker', 'refresh_time')
        self.__maintainCheckerDaemonSleep = config.get('maintain_checker', 'daemon_sleep')

        self.__kwparams = {
            'eventMgrPort': self.__eventMgrPort,
            'eventMgrPage': self.__eventMgrPage,
            'maintainCheckerReplTime': self.__maintainCheckerReplTime,
            'maintainCheckerRefreshTime': self.__maintainCheckerRefreshTime,
            'maintainCheckerDaemonSleep': self.__maintainCheckerDaemonSleep,
        }

    def getKwparams(self):
        return self.__kwparams
if __name__ == '__main__':
    SC = SwiftCfg("%s/DCloudSwift/Swift.ini" % BASEDIR)
    kwparams = SC.getKwparams()

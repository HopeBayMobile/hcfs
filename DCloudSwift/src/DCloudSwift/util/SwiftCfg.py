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

        self.__logLevel = config.get('log', 'level')

        self.__username = "root"

        self.__password = config.get('storage', 'password')
        self.__devicePrx = "sdb"
        self.__proxyPort = int(config.get('storage', 'proxyPort'))

        self.__portalUrl = config.get('portal', 'url')

        self.__kwparams = {
            'logLevel': self.__logLevel,
            'username': self.__username,
            'password': self.__password,
            'devicePrx': self.__devicePrx,
            'proxyPort': self.__proxyPort,
            'portalUrl': self.__portalUrl
        }

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
        self.__maintainReplTime = config.get('maintain', 'replication_time')
        self.__maintainRefreshTime = config.get('maintain', 'refresh_time')
        self.__maintainDaemonSleep = config.get('maintain', 'daemon_sleep')

        self.__kwparams = {
            'eventMgrPort': self.__eventMgrPort,
            'eventMgrPage': self.__eventMgrPage,
            'maintainReplTime': self.__maintainReplTime,
            'maintainRefreshTime': self.__maintainRefreshTime,
            'maintainDaemonSleep': self.__maintainDaemonSleep,
        }

    def getKwparams(self):
        return self.__kwparams
if __name__ == '__main__':
    SC = SwiftCfg("%s/DCloudSwift/Swift.ini" % BASEDIR)
    kwparams = SC.getKwparams()

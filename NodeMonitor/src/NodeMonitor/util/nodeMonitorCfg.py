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

class NodeMonitorCfg:
    def __init__(self, configFile):
        self.__kwparams = {}
        self.__configFile = configFile

        config = ConfigParser()
        config.readfp(open(self.__configFile))

        self.__logDir = config.get('log', 'dir')
        self.__logName = config.get('log', 'name')
        self.__logLevel = config.get('log', 'level')
        self.__receiverUrl = config.get('receiver', 'url')
        self.__min = int(config.get('interval', 'min'))
        self.__max = int(config.get('interval', 'max'))
        self.__interval = (self.__min, self.__max)

        self.__kwparams = {
            'logDir': self.__logDir,
            'logLevel': self.__logLevel,
            'logName': self.__logName,
            'receiverUrl': self.__receiverUrl,
            'interval': self.__interval,
        }

        os.system("mkdir -p " + self.__kwparams['logDir'])
        os.system("touch " + self.__kwparams['logDir'] + "/" + self.__kwparams['logName'])

    def getKwparams(self):
        return self.__kwparams


if __name__ == '__main__':
    NMC = NodeMonitorCfg("/etc/delta/node_monitor.ini")
    kwparams = NMC.getKwparams()
    print kwparams

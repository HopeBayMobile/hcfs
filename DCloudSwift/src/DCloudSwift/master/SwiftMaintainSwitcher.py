#!/usr/bin/python -u
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Mode Switcher (for Swift maintenance)

import os
import sys
import time
import socket
import random
import pickle
import signal
import simplejson as json

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util.daemon import Daemon
from util.util import GlobalVar
from util.SwiftCfg import SwiftMasterCfg
from util import util
from util.database import NodeInfoDatabaseBroker

from common.events import HDD
from common.events import HEARTBEAT




class SwiftMaintainSwitcher(Daemon):
    """
    SwiftMaintainSwitcher is a daemon which will pulling event database
    in a period time.  It will handle disk and node failed event and
    decide which nodes or which disks need to maintain.
    Those nodes or disks will add or modify on maintain table.
    """

    def __init__(self, pidfile, DBFile=None, replicationTime=None,
                 refreshTime=None, daemonSleep=None):
        """
        construct SwiftMaintainSwitcher daemon and create pid file
        it will read replication_time from swift_master.ini
        @type pidfile: string
        @param pidfile: pid file path
        @type DBFile: string
        @param DBFile: sqlite db file abstract path
        @type replicationTime: integer
        @param replicationTime: waiting time for replication
        @type refreshTime: integer
        @param refreshTime: after refresh time, we will claim a node is missing.
        @type daemonSleep: integer
        @param daemonSleep: the daemon sleep time
        """
        Daemon.__init__(self, pidfile)
        logger = util.getLogger(name='swiftmaintainswitcher')
        self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        if DBFile is None:
            self.DBFile = SwiftMasterCfg(GlobalVar.NODE_DB)
        else:
            self.DBFile = DBFile
        if replicationTime is None:
            self.replicationTime = int(self.masterCfg \
                .getKwparams()['maintainReplTime'])
        else:
            self.replicationTime = replicationTime
        if refreshTime is None:
            self.refreshTime = int(self.masterCfg \
                .getKwparams()['maintainRefreshTime'])
        else:
            self.refreshTime = refreshTime
        if daemonSleep is None:
            self.daemonSleep = int(self.masterCfg \
                .getKwparams()['maintainDaemonSleep'])
        else:
            self.daemonSleep = daemonSleep
        self.db = NodeInfoDatabaseBroker(self.DBFile)

    def checkService(self):
        """
        implement status change logic for service mode to waiting mode
        """
        logger = util.getLogger(name='swiftmaintainswitcher.checkService')
        self.nowtimeStamp = int(time.mktime(time.localtime()))
        logger.info("start check hostname which is in service mode")
        serviceNode = self.db.query_node_info_table('mode = "service"').fetchall()
        for row in serviceNode:
            if row[1] == HEARTBEAT.status[2]:
                record = self.updateStatusWaiting(row[0])
                logger.info("the record which update status to waiting: %s"
                            % str(self.dict_from_row(record)))
                continue
            if row[1] == HEARTBEAT.status[0]:
                diskInfo = json.loads(row[3])
                if ((self.refreshTime + row[2]) < self.nowtimeStamp):
                    record = self.updateStatusWaiting(row[0])
                    logger.info("the record which update status to waiting: %s"
                            % str(self.dict_from_row(record)))
                    continue
                if (('missing' in diskInfo) or ('broken' in diskInfo)):
                    if (diskInfo['timestamp']
                            + self.replicationTime > self.nowtimeStamp):
                        record = self.updateStatusWaiting(row[0])
                        logger.info("the record which update status to waiting: %s"
                                    % str(self.dict_from_row(record)))
                        continue
        logger.info("end check hostname which is in service mode")

    def updateStatusService(self, hostname=None):
        """
        update node status from waiting to service
        @rtype: boolean
        @return: Return False if update fail.
                 Return raw data if update success.
        """
        return self.updateStatus(hostname, 'service')

    def updateStatusWaiting(self, hostname=None):
        """
        update node status from service to waiting
        @rtype: boolean
        @return: Return False if update fail.
                 Return raw data if update success.
        """
        return self.updateStatus(hostname, 'waiting')

    def updateStatus(self, hostname, status):
        """
        can update status by hostname
        @rtype: boolean
        @return: Return False if hostname is None.
                 Return raw data if update success.
        """
        if hostname is None:
            return False
        try:
            return db.update_node_mode(hostname, status, self.nowtimeStamp)
        except sqlite3.Error, e:
            logger.error('sqlite update error: %s' % e.args[0])
            return False

    def checkWaiting(self):
        """
        implement status change logic for waiting mode to service mode
        """
        logger = util.getLogger(name='swiftmaintainswitcher.checkWaiting')
        self.nowtimeStamp = int(time.mktime(time.localtime()))
        logger.info("start check hostname which is in waiting mode")
        waitingNode = self.db.query_node_info_table('mode = "waiting"').fetchall()
        for row in waitingNode:
            if row[1] == HEARTBEAT.status[0]:
                diskInfo = json.loads(row[3])
                if (('missing' not in diskInfo) or ('broken' not in diskInfo)):
                    record = self.updateStatusService(row[0])
                    logger.info("the record which update status to waiting: %s"
                                    % str(self.dict_from_row(record)))
                    continue
        logger.info("end check hostname which is in waiting mode")

    def dict_from_row(self, row):
        dt = {}
        for clName in row.keys():
            dt[clName] = row[clName]
        return dt 

    def run(self):
        """
        start daemon and check service and waiting mode
        """
        logger = util.getLogger(name='swiftmaintainswitcher.run')
        while True:
            logger.error("daemon sleep: %s" % self.daemonSleep)
            self.checkService()
            self.checkWaiting()
            time.sleep(int(self.daemonSleep))


def main(DBFile=None):
    daemon = SwiftMaintainSwitcher('/var/run/SwiftMaintainSwitcher.pid', DBFile)
    if len(sys.argv) == 2:
        if 'start' == sys.argv[1]:
            print "daemon start"
            daemon.start()
        elif 'stop' == sys.argv[1]:
            daemon.stop()
        elif 'restart' == sys.argv[1]:
            daemon.restart()
        else:
            print "Unknown command"
            sys.exit(2)
        sys.exit(0)
    else:
        print "usage: %s start|stop|restart" % sys.argv[0]
        sys.exit(2)

if __name__ == "__main__":
    DBFile = '/etc/test/test.db'
    os.system("rm -f %s" % DBFile)
    db = NodeInfoDatabaseBroker(DBFile)
    db.initialize()
    timestamp = int(time.mktime(time.localtime()))
    diskInfo = {
                    'timestamp': timestamp,
                    'missing': {
                                    'count': 6,
                                    'timestamp': timestamp,
                                },
                    'broken': [
                               {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                               },
                              ],
                    'healthy': [
                                {
                                    'SN': 'aaaaa',
                                    'timestamp': timestamp,
                                }
                               ],
                }
    row = db.add_node('192.168.1.20', 'dead', timestamp-100, json.dumps(diskInfo), 'service', timestamp)
    row = db.add_node('192.168.1.100', 'alive', timestamp-100, '{}', 'waiting', timestamp)
    main(DBFile)
    
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

from util.daemon import Daemon
from util.util import GlobalVar
from util.SwiftCfg import SwiftMasterCfg
from util import util

from common import *

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)


class SwiftMaintainSwitcher(Daemon):
    """
    SwiftMaintainSwitcher is a daemon which will pulling event database
    in a period time.  It will handle disk and node failed event and
    decide which nodes or which disks need to maintain.
    Those nodes or disks will add or modify on maintain table.
    """

    def __init__(self, pidfile, replicationTime=None,
                 refreshTime=None, daemonSleep=None):
        """
        construct SwiftMaintainSwitcher daemon and create pid file
        it will read replication_time from swift_master.ini
        @type pidfile: string
        @param pidfile: pid file path
        """
        Daemon.__init__(self, pidfile)
        logger = util.getLogger(name='swiftmaintainswitcher')
        self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        if replicationTime is None:
            self.replicationTime = self.masterCfg \
                .getKwparams()['maintainCheckerReplTime']
        else:
            self.replicationTime = replicationTime
        if refreshTime is None:
            self.refreshTime = self.masterCfg \
                .getKwparams()['maintainCheckerRefreshTime']
        else:
            self.refreshTime = refreshTime
        if daemonSleep is None:
            self.daemonSleep = self.masterCfg \
                .getKwparams()['maintainCheckerDaemonSleep']
        else:
            self.daemonSleep = daemonSleep
        self.db = NodeInfoDatabaseBroker()

    def checkService(self):
        """
        implement status change logic for service mode to waiting mode
        """
        logger = util.getLogger(name='swiftmaintainswitcher.checkService')
        self.nowtimeStamp = int(time.mktime(time.localtime()))
        serviceNode = self.db.show_node_info_table()
        for row in serviceNode:
            print row
            if row[1] == HEARTBEAT.status[2]:
                self.updateStatusWaiting(row[0])
                continue
            if row[1] == HEARTBEAT.status[0]:
                diskInfo = json.loads(row[3])
                if ((self.refreshTime + row[2]) < self.nowtimeStamp):
                    self.updateStatusWaiting(row[0])
                    continue
                if (('missing' in diskInfo) or ('broken' in diskInfo)):
                    if (diskInfo['timestamp']
                            + self.replicationTime > self.nowtimeStamp):
                        self.updateStatusWaiting(row[0])
                        continue

    def updateStatusService(self, hostname=None):
        """
        update node status from waiting to service
        @rtype: boolean
        @return: Return False if update fail.
                 Return True if update success.
        """
        return self.updateStatus(hostname, 'service')

    def updateStatusWaiting(self, hostname=None):
        """
        update node status from service to waiting
        @rtype: boolean
        @return: Return False if update fail.
                 Return True if update success.
        """
        return self.updateStatus(hostname, 'waiting')

    def updateStatus(self, hostname, status):
        """
        can update status by hostname
        @rtype: boolean
        @return: Return False if hostname is None.
                 Return True if update success.
        """
        if hostname is None:
            return False
        try:
            db.update_node_mode(hostname, status, self.nowtimeStamp)
            return True
        except sqlite3.Error, e:
            logger.error('sqlite update error: %s' % e.args[0])
            return False

    def checkWaiting(self):
        logger = util.getLogger(name='swiftmaintainswitcher.checkWaiting')
        self.nowtimeStamp = int(time.mktime(time.localtime()))
        waitingNode = self.db.show_node_info_table()
        for row in waitingNode:
            print row
            if row[1] == HEARTBEAT.status[0]:
                diskInfo = json.loads(row[3])
                if (('missing' not in diskInfo) or ('broken' not in diskInfo)):
                    self.updateStatusService(row[0])
                    continue

    def run(self):
        """
        start daemon and check service and waiting mode
        """
        logger = util.getLogger(name='swiftmaintainswitcher.run')
        while (True):
            self.checkService()
            self.checkWaiting()
            time.sleep(self.daemonSleep)


def main():
    daemon = SwiftMaintainSwitcher('/var/run/SwiftMaintainSwitcher.pid')
    if len(sys.argv) == 2:
        if 'start' == sys.argv[1]:
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
    main()
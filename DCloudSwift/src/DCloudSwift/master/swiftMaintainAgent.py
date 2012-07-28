#!/usr/bin/python -u
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Swift Maintenance Agent

import os
import sys
import time
import socket
import random
import pickle
import signal
import json
import sqlite3

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util.daemon import Daemon
from util.util import GlobalVar
from util.SwiftCfg import SwiftMasterCfg
from util.database import MaintenanceBacklogDatabaseBroker
from util.database import NodeInfoDatabaseBroker
from util import util
from common.events import HDD
from common.events import HEARTBEAT


class SwiftMaintainAgent(Daemon):
    """
    SwiftMaintainAgent is a daemon which will pulling event database
    in a period time.  It will handle disk and node failed event and
    decide which nodes or which disks need to maintain.
    Those nodes or disks will add or modify on maintain table.
    """

    def __init__(self, pidfile, replicationTime=None,
                 refreshTime=None, daemonSleep=None):
        """
        construct SwiftMaintainAgent daemon and create pid file
        it will read replication_time from swift_master.ini
        @type pidfile: string
        @param pidfile: pid file path
        """
        Daemon.__init__(self, pidfile)
        logger = util.getLogger(name='swiftmaintainagent')
        self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        if replicationTime is None:
            self.replicationTime = self.masterCfg.getKwparams()['maintainReplTime']
        else:
            self.replicationTime = replicationTime
        self.replicationTime = int(self.replicationTime)
       
        if refreshTime is None:
            self.refreshTime = self.masterCfg.getKwparams()['maintainRefreshTime'] 
        else:
            self.refreshTime = refreshTime
        self.refreshTime = int(self.refreshTime)

        if daemonSleep is None:
            self.daemonSleep = self.masterCfg.getKwparams()['maintainDaemonSleep']
        else:
            self.daemonSleep = daemonSleep
        self.daemonSleep = int(self.daemonSleep)

        self.backlog = MaintenanceBacklogDatabaseBroker(GlobalVar.MAINTENANCE_BACKLOG)
        self.nodeInfo = NodeInfoDatabaseBroker(GlobalVar.NODE_DB)


    @staticmethod
    def isBacklogEmpty(backlog):
        """
        Check whether the maintenance_backlog is empty.
        """
        logger = util.getLogger(name='swiftmaintainagent.checkBacklog')
        nodeList = backlog.show_maintenance_backlog_table()

        if len(nodeList) > 0:
            return False
        else:
            return True

    @staticmethod
    def computeDisks2Reserve(disk_info, deadline):
        """
        compute disks to reserve
        @param disk_info: disk information of a node in node_info table
        @return: disks should be reserved
        """
        logger = util.getLogger(name='swiftmaintainagent.computeDisks2Reserve')

        disks_to_reserve = [disk["SN"] for disk in disk_info["healthy"] if disk["SN"]]

        for disk in disk_info["broken"]:
            if disk["timestamp"] > deadline:
                if disk["SN"]:
                    disks_to_reserve.append(disk["SN"])

        return disks_to_reserve

    @staticmethod
    def computeDisks2Replace(disk_info, deadline):
        """
        compute disks to replace
        @param disk_info: disk information of a node in node_info table
        @return: disks should be reserved
        """
        logger = util.getLogger(name='swiftmaintainagent.computeDisks2Replace')

        disks_to_replace = []

        for disk in disk_info["broken"]:
            if disk["timestamp"] <= deadline:
                if disk["SN"]:
                    disks_to_replace.append(disk["SN"])

        return disks_to_replace

    @staticmethod
    def incrementBacklog(nodeInfo, backlog, deadline):
        """
        Add a waiting node to the maintenance backlog
        @return: the row added to the backlog   
        """
        logger = util.getLogger(name='swiftmaintainagent.incrementBacklog')
        nodeList = nodeInfo.query_node_info_table("mode='waiting'").fetchall()

        if len(nodeList) == 0:
            return None
        else:
            ret = None
            for node in nodeList:
                    ret = SwiftMaintainAgent.computeMaintenanceTask(node, deadline)
                    if ret:
                        row = backlog.add_maintenance_task(target=ret["target"],
                                                           hostname=ret["hostname"], 
                                                           disks_to_reserve=ret["disks_to_reserve"], 
                                                           disks_to_replace=ret["disks_to_replace"])
                        if row:
                            return row

    @staticmethod
    def computeMaintenanceTask(node, deadline):
        """
        @param node: a raw in the node_info table
        @param deadline: deadline of disk errors
        @return: return a maintenance task
        """
        logger = util.getLogger(name='swiftmaintainagent.computeMaintenanceTask')
        disk_info = json.loads(node["disk"])

        ret = {
                "target": None,
                "hostname": node["hostname"],
                "disks_to_reserve": None,
                "disks_to_replace": None,
              }

        if node["status"] == "dead":
            ret["target"] = "node_missing"
        elif disk_info["missing"]["timestamp"] > deadline:
            ret = None
        elif disk_info["missing"]["count"] != 0:
            ret["target"] = "disk_missing"
            ret["disks_to_reserve"] = json.dumps(SwiftMaintainAgent.computeDisks2Reserve(disk_info, deadline))
        elif len(disk_info["broken"]) > 0:
            ret["target"] = "disk_broken"
            ret["disks_to_replace"] = json.dumps(SwiftMaintainAgent.computeDisks2Replace(disk_info, deadline))
        else:
            ret = None

        return ret

    @staticmethod
    def updateMaintenanceBacklog(nodeInfo, backlog, deadline):
        """
        update tasks in the maintenance backlog
        @return: None   
        """
        logger = util.getLogger(name='swiftmaintainagent.updateMaintenanceBacklog')
        tasks = backlog.show_maintenance_backlog_table()

        for task in tasks:
            hostname = task["hostname"]
            node = nodeInfo.get_info(hostname)
            backlog.delete_maintenance_task(hostname)
            if not node:
                continue

            ret = SwiftMaintainAgent.computeMaintenanceTask(node, deadline)
            if not ret:
                continue
            
            backlog.add_maintenance_task(target=ret["target"],
                                         hostname=ret["hostname"], 
                                         disks_to_reserve=ret["disks_to_reserve"], 
                                         disks_to_replace=ret["disks_to_replace"])

    def run(self):
        """
        start daemon and check service and waiting mode
        """
        logger = util.getLogger(name='swiftmaintainagent.run')
        while (True):
            try:
                deadline = int(time.time() - (self.replicationTime)*3600)
                SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=self.nodeInfo,
                                                            backlog=self.backlog,
                                                            deadline=deadline)

                if SwiftMaintainAgent.isBacklogEmpty(backlog=self.backlog):  # check whether the maintenance_backlog is empty. (C1)
                    SwiftMaintainAgent.incrementBacklog(nodeInfo=self.nodeInfo, 
                                                        backlog=self.backlog, 
                                                        deadline=deadline) # choose a node for repair (P1)

            except Exception as e:
                logger.error(str(e))

            time.sleep(self.daemonSleep)


def main():
    daemon = SwiftMaintainAgent('/var/run/SwiftMaintainAgent.pid')
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

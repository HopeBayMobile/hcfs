#!/usr/bin/python -u
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Swift Maintenance Agent

import os
import sys
import time
import json
import sqlite3

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.insert(0,"%s/DCloudSwift/" % BASEDIR)
#sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util.daemon import Daemon
from util.util import GlobalVar
from util.SwiftCfg import SwiftMasterCfg
from util.database import MaintenanceBacklogDatabaseBroker
from util.database import NodeInfoDatabaseBroker
from util import util
from common.events import HDD
from common.events import HEARTBEAT


lockFile = "/etc/delta/swift_maintain.lock"


# tryLock decorator
def tryLock(tries=3, lockLife=900):
    def deco_tryLock(fn):
        def wrapper(*args, **kwargs):
            returnVal = None
            locked = 1
            try:
                os.system("mkdir -p %s" % os.path.dirname(lockFile))
                cmd = "lockfile -11 -r %d -l %d %s" % (tries, lockLife, lockFile)
                locked = os.system(cmd)
                if locked == 0:
                    returnVal = fn(*args, **kwargs)  # first attempt
                else:
                    raise TryLockError()
                return returnVal
            finally:
                if locked == 0:
                    os.system("rm -f %s" % lockFile)

        return wrapper  # decorated function

    return deco_tryLock  # @retry(arg[, ...]) -> true decorator


class TryLockError(Exception):
    def __str__(self):
        return "Failed to tryLock lockFile"


class SwiftMaintainAgent:

    def __init__(self, replicationTime=None):
        """
        construct SwiftMaintainAgent object.
        It will read replication_time from swift_master.ini by default
        """
        self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        if replicationTime is None:
            self.replicationTime = float(self.masterCfg.getKwparams()['maintainReplTime']) * 3600
        else:
            self.replicationTime = replicationTime
        self.replicationTime = int(self.replicationTime)

        self.backlog = MaintenanceBacklogDatabaseBroker(GlobalVar.MAINTENANCE_BACKLOG)
        self.nodeInfo = NodeInfoDatabaseBroker(GlobalVar.NODE_DB)

    @staticmethod
    def isBacklogEmpty(backlog):
        """
        Check whether the maintenance_backlog is empty.
        """
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
            if not json.loads(ret["disks_to_replace"]):
                ret = None
        else:
            ret = None

        return ret

    @staticmethod
    def updateMaintenanceBacklog(nodeInfo, backlog, deadline):
        """
        update tasks in the maintenance backlog
        @return: None
        """
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

    @staticmethod
    @tryLock()
    def renewMaintenanceBacklog(replicationTime=None):
        """
        1. update tasks in the maintenance backlog
        2. add a new task to the maintance backlog if empty
        @return: {code: <integer>, message:<string>}
        """

        logger = util.getLogger(name='swiftmaintainagent.renewMaintenanceBacklog')

        masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)

        if replicationTime is None:
            replicationTime = float(masterCfg.getKwparams()['maintainReplTime']) * 3600
        replicationTime = int(replicationTime)

        backlog = MaintenanceBacklogDatabaseBroker(GlobalVar.MAINTENANCE_BACKLOG)
        nodeInfo = NodeInfoDatabaseBroker(GlobalVar.NODE_DB)

        try:
            deadline = int(time.time() - replicationTime)
            SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=nodeInfo,
                                                        backlog=backlog,
                                                        deadline=deadline)

            if SwiftMaintainAgent.isBacklogEmpty(backlog=backlog):  # check whether the maintenance_backlog is empty. (C1)
                SwiftMaintainAgent.incrementBacklog(nodeInfo=nodeInfo,
                                                    backlog=backlog,
                                                    deadline=deadline)  # choose a node for repair (P1)

            return {"code": 0, "message": True}
        except Exception as e:
            logger.error(str(e))
            return {"code": 1, "message": str(e)}


def main():
    print SwiftMaintainAgent.renewMaintenanceBacklog()

if __name__ == "__main__":
    main()

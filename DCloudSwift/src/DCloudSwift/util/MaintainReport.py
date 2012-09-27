#!/usr/bin/python -u
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

import os
import pprint
import sys
import simplejson as json

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from daemon import Daemon
from util import GlobalVar
from mongodb import get_mongodb
from SwiftCfg import SwiftMasterCfg
from database import MaintenanceBacklogDatabaseBroker
from database import NodeInfoDatabaseBroker
from master.swiftMaintainAgent import SwiftMaintainAgent
from datetime import datetime

class MaintainReport():
    """
    query maintain table and generate report
    """
    def __init__(self, DBFile=None):
        """
        initialize MaintainReport
        @type DBFile: string
        @param DBFile: DB file abstract path
        """
        self.report = {}
        if DBFile is None:
            self.DBFile = GlobalVar.MAINTENANCE_BACKLOG
        else:
            self.DBFile = DBFile
        self.db = MaintenanceBacklogDatabaseBroker(self.DBFile)

    def query(self, target=None):
        """
        query maintain table by target name
        @type target: string
        @param target: target name in maintain table
        @rtype: False or string
        @return: Return raw data.
        """
        result = None
        if target is None:
            result = self.db.show_maintenance_backlog_table()
        else:
            result = self.db.query_maintenance_backlog_table(
                'target = "%s"' % target)
        for row in result:
            print row

    def renew_backlog(self):
        """
        1. update tasks in the maintenance backlog
        2. add a new task to the maintance backlog if empty
        @return: {code: <integer>, message:<string>}   
        """
        return SwiftMaintainAgent.renewMaintenanceBacklog()

    def get_backlog(self):
        """
        query maintain table 
        @return: Return raw data.
        """
        result = self.db.show_maintenance_backlog_table()
        return result

def print_maintenance_backlog():
    '''
    Command line implementation of node info initialization.
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_print_backlog
    arguments:
        None
    '''

    if (len(sys.argv) != 1):
        print >> sys.stderr, Usage
        sys.exit(1)

    if not os.path.exists("/var/run/SwiftMaintainSwitcher.pid"):
        print >> sys.stderr, "Cannot find pidfile of swift-maintain-switcher. Is it running?"
        sys.exit(1)

    if not os.path.exists("/var/run/SwiftEventMgr.pid"):
        print >> sys.stderr, "Cannot find pidfile of swift-event-manager. Is it running?"
        sys.exit(1)

    try:
        mr = MaintainReport()
        ret = mr.renew_backlog()
        if ret["code"] !=0:
            print >> sys.stderr, ret["message"]
            sys.exit(1)

        backlog = mr.get_backlog()
        for task in backlog:
            output = "%s:{\n" % task["hostname"]
            output += "    target: %s\n" % task["target"]
            if task["target"] == "disk_missing":
                output += "    disks_to_reserve: %s\n" % task["disks_to_reserve"]
            elif task["target"] == "disk_broken":
                output += "    disks_to_replace: %s\n" % task["disks_to_replace"]

            output+="}"
            print output
    except Exception as e:
        print >> sys.stderr, str(e)
        sys.exit(1)

def print_node_info():
    '''
    Command line implementation of printing node info.
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_print_node_info
    arguments:
        None
    '''

    if (len(sys.argv) != 1):
        print >> sys.stderr, Usage
        sys.exit(1)

    try:
        DBFile = GlobalVar.NODE_DB
        db = NodeInfoDatabaseBroker(DBFile)
        nodes = db.show_node_info_table()

        for node in nodes:
            disk_list = json.loads(node["disk"])
            output = "%s:{\n" % node["hostname"]
            output += "  status: %s\n" % node["status"]
            output += "  timestamp: %d\n" % node["timestamp"]
            output += "  disk: %s\n" % node["disk"]
            output += "  mode: %s\n" % node["mode"]
            output += "  switchpoint: %s\n" % node["switchpoint"]
            output+="}\n"
            print output

    except Exception as e:
        print >> sys.stderr, str(e)
        sys.exit(1)

def print_runtime_info():
    '''
    Command line implementation of printing runtime information.
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_print_runtime_info
    arguments:
        None
    '''

    if (len(sys.argv) != 1):
        print >> sys.stderr, Usage
        sys.exit(1)

    try:
        db = get_mongodb(GlobalVar.MONITOR_MONGODB)
        nodes = db.runtime_info.find()

        for node in nodes:
            date = datetime.fromtimestamp(node["timestamp"])
            data = node["data"]
            data[u"date"] = str(date)
            data = {node["hostname"]: data}
            pprint.pprint(data, indent=1)

    except Exception as e:
        print >> sys.stderr, str(e)
        sys.exit(1)

def main(DBFile=None):
    maintainReport = MaintainReport(DBFile)
    maintainReport.query()

if __name__ == '__main__':
    DBFile = None
    main(DBFile)

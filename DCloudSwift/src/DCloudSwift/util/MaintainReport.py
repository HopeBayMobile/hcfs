#!/usr/bin/python -u
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

import os
import sys
import simplejson as json

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from daemon import Daemon
from util import GlobalVar
from SwiftCfg import SwiftMasterCfg
from database import MaintenanceBacklogDatabaseBroker


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

    try:
        mr = MaintainReport()
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

def main(DBFile=None):
    maintainReport = MaintainReport(DBFile)
    maintainReport.query()

if __name__ == '__main__':
    DBFile = None
#    if len(sys.argv) == 2:
#        if 'test' == sys.argv[1]:
#            DBFile = '/etc/test/test.db'
#            os.system("rm -f %s" % DBFile)
#            db = MaintenanceBacklogDatabaseBroker(DBFile)
#            db.initialize()
#            timestamp = int(time.mktime(time.localtime()))
#            diskReserve = \
#                ['SN_reserve_001', 'SN_reserve_002', 'SN_reserve_003']
#            diskReplace = ['SN_replace001', 'SN_replace002', 'SN_replace003']
#            row = db.add_maintenance_task('node_missing', '192.168.2.30',
#                        json.dumps(diskReserve), json.dumps(diskReplace))
    main(DBFile)

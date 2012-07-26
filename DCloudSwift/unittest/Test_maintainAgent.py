import nose
import sys
import os
import json
import time

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util.util import GlobalVar
from util.database import NodeInfoDatabaseBroker
from util.database import MaintenanceBacklogDatabaseBroker
from master.swiftEventMgr import SwiftEventMgr
from master.swiftMaintainAgent import SwiftMaintainAgent
from datetime import datetime
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))


class Test_isBacklogEmpty:
    '''
    Test the function SwiftEventMgr.isBacklogEmpty
    '''
    def setup(self):
        if os.path.exists("/etc/test/nodeInfo.db"):
            os.system("rm /etc/test/nodeInfo.db")
        if os.path.exists("/etc/test/backlog.db"):
            os.system("rm /etc/test/backlog.db")

        self.backup = GlobalVar.ORI_SWIFTCONF+".bak"
        os.system("cp %s %s" % (GlobalVar.ORI_SWIFTCONF, self.backup))
        os.system("cp %s/fake6.ini %s" % (WORKING_DIR, GlobalVar.ORI_SWIFTCONF))
        self.nodeInfo = NodeInfoDatabaseBroker("/etc/test/nodeInfo.db")
        self.nodeInfo.initialize()

        self.backlog = MaintenanceBacklogDatabaseBroker("/etc/test/backlog.db")
        self.backlog.initialize()

    def test_empty(self):
        ret = SwiftMaintainAgent.isBacklogEmpty(self.backlog)
        nose.tools.ok_(ret == True)

    def test_non_empty(self):
        self.backlog.add_maintenance_task(target="node_missing", hostname="TPEAAA", 
                                          disks_to_reserve=None, disks_to_replace=None)

        ret = SwiftMaintainAgent.isBacklogEmpty(self.backlog)
        nose.tools.ok_(ret == False)

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))
        os.system("rm /etc/test/nodeInfo.db")
        os.system("rm /etc/test/backlog.db")

class Test_computeDisks2Reserve:
    '''
    Test the function SwiftEventMgr.computeDisks2Reserve
    '''
    def setup(self):
        if os.path.exists("/etc/test/nodeInfo.db"):
            os.system("rm /etc/test/nodeInfo.db")
        if os.path.exists("/etc/test/backlog.db"):
            os.system("rm /etc/test/backlog.db")

        self.backup = GlobalVar.ORI_SWIFTCONF+".bak"
        os.system("cp %s %s" % (GlobalVar.ORI_SWIFTCONF, self.backup))
        os.system("cp %s/fake6.ini %s" % (WORKING_DIR, GlobalVar.ORI_SWIFTCONF))
        self.nodeInfo = NodeInfoDatabaseBroker("/etc/test/nodeInfo.db")
        self.nodeInfo.initialize()

        self.backlog = MaintenanceBacklogDatabaseBroker("/etc/test/backlog.db")
        self.backlog.initialize()

    def test_boundary(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 0,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                     {"SN": "3", "timestamp": 600},
                                   ],

                         "healthy":[
                                     {"SN": "4", "timestamp": 400},
                                     {"SN": "5", "timestamp": 997},
                                     {"SN": "6", "timestamp": 996},
                                   ]
        }

        disks2reserve = SwiftMaintainAgent.computeDisks2Reserve(disk_info=disk_info, 
                                                                deadline=500)

        

        nose.tools.ok_(sorted(disks2reserve) == ["3","4","5","6"])

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))
        os.system("rm /etc/test/nodeInfo.db")
        os.system("rm /etc/test/backlog.db")

class Test_computeDisks2Replace:
    '''
    Test the function SwiftEventMgr.computeDisks2Reserve
    '''
    def setup(self):
        if os.path.exists("/etc/test/nodeInfo.db"):
            os.system("rm /etc/test/nodeInfo.db")
        if os.path.exists("/etc/test/backlog.db"):
            os.system("rm /etc/test/backlog.db")

        self.backup = GlobalVar.ORI_SWIFTCONF+".bak"
        os.system("cp %s %s" % (GlobalVar.ORI_SWIFTCONF, self.backup))
        os.system("cp %s/fake6.ini %s" % (WORKING_DIR, GlobalVar.ORI_SWIFTCONF))
        self.nodeInfo = NodeInfoDatabaseBroker("/etc/test/nodeInfo.db")
        self.nodeInfo.initialize()

        self.backlog = MaintenanceBacklogDatabaseBroker("/etc/test/backlog.db")
        self.backlog.initialize()

    def test_boundary(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 0,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                     {"SN": "3", "timestamp": 600},
                                     {"SN": "4", "timestamp": 400},
                                   ],

                         "healthy":[
                                     {"SN": "5", "timestamp": 997},
                                     {"SN": "6", "timestamp": 996},
                                   ]
        }

        disks2replace = SwiftMaintainAgent.computeDisks2Replace(disk_info=disk_info, 
                                                                deadline=500)

        

        nose.tools.ok_(sorted(disks2replace) == ["1","2","4"])

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))
        os.system("rm /etc/test/nodeInfo.db")
        os.system("rm /etc/test/backlog.db")

if __name__ == "__main__":
    pass

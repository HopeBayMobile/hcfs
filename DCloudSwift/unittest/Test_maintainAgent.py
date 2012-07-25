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
    Test the function SwiftEventMgr.getExpectedDeviceCnt
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

if __name__ == "__main__":
    pass

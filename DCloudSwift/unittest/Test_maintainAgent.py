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

class Test_computeMaintenanceTask:
    '''
    Test the function SwiftEventMgr.computeMaintenanceTask
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

    def test_node_missing(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 3,
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "dead",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)

        task = SwiftMaintainAgent.computeMaintenanceTask(node=node, 
                                                         deadline=500)

        nose.tools.ok_(task["target"]=="node_missing")

    def test_disk_missing(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 3,
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)

        task = SwiftMaintainAgent.computeMaintenanceTask(node=node, 
                                                         deadline=500)

        nose.tools.ok_(task["target"]=="disk_missing")
        disks2reserve = json.loads(task["disks_to_reserve"])
        nose.tools.ok_(sorted(disks2reserve)==["3","4","5","6"])
        nose.tools.ok_(task["disks_to_replace"] is None)

    def test_disk_broken(self):
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)

        task = SwiftMaintainAgent.computeMaintenanceTask(node=node, 
                                                         deadline=500)

        nose.tools.ok_(task["target"]=="disk_broken")
        nose.tools.ok_(task["disks_to_reserve"] is None)
        disks2replace = json.loads(task["disks_to_replace"])
        nose.tools.ok_(sorted(disks2replace)==["1","2"])

    def test_new_disk_missing(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 3,
                                        "timestamp": 501,
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)

        task = SwiftMaintainAgent.computeMaintenanceTask(node=node, 
                                                         deadline=500)

        nose.tools.ok_(task is None)

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))
        os.system("rm /etc/test/nodeInfo.db")
        os.system("rm /etc/test/backlog.db")

class Test_incrementBacklog:
    '''
    Test the function SwiftEventMgr.incrementBacklog
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

    def test_node_missing_waiting_node(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 3,
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "dead",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "waiting",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)
        

        task = SwiftMaintainAgent.incrementBacklog(nodeInfo=self.nodeInfo,
                                                   backlog=self.backlog, 
                                                   deadline=500)

        nose.tools.ok_(task["hostname"]=="ThinkPad")
        nose.tools.ok_(task["target"]=="node_missing")

    def test_disk_missing_waiting_node(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 3,
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "waiting",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)
        
        task = SwiftMaintainAgent.incrementBacklog(nodeInfo=self.nodeInfo,
                                                   backlog=self.backlog, 
                                                   deadline=500)

        nose.tools.ok_(task["hostname"]=="ThinkPad")
        nose.tools.ok_(task["target"]=="disk_missing")
        disks2reserve = json.loads(task["disks_to_reserve"])
        nose.tools.ok_(sorted(disks2reserve) == ["3","4","5","6"])
        nose.tools.ok_(task["disks_to_replace"] is None)

    def test_disk_broken_waiting_node(self):
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "waiting",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)
        
        task = SwiftMaintainAgent.incrementBacklog(nodeInfo=self.nodeInfo,
                                                   backlog=self.backlog, 
                                                   deadline=500)

        nose.tools.ok_(task["hostname"]=="ThinkPad")
        nose.tools.ok_(task["target"]=="disk_broken")
        disks2replace = json.loads(task["disks_to_replace"])
        nose.tools.ok_(sorted(disks2replace) == ["1","2"])
        nose.tools.ok_(task["disks_to_reserve"] is None)

    def test_duplicate_tasks(self):
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

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info),
                  "mode": "waiting",
                  "switchpoint": 123,
        }

        node = self.nodeInfo.add_node(**node)
        
        self.backlog.add_maintenance_task(target="node_missing", hostname="ThinkPad",
                                          disks_to_reserve=None, disks_to_replace=None)

        task = SwiftMaintainAgent.incrementBacklog(nodeInfo=self.nodeInfo,
                                                   backlog=self.backlog, 
                                                   deadline=500)

        nose.tools.ok_(task is None)

    def test_multi_waiting_nodes_with_duplicate_tasks(self):
        disk_info_1 = {
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

        node_1 = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info_1),
                  "mode": "waiting",
                  "switchpoint": 123,
        }

        disk_info_2 = {
                         "timestamp": 1100,
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

        node_2 = {
                  "hostname": "ThinkPad2",
                  "status": "alive",
                  "timestamp": 123,
                  "disk": json.dumps(disk_info_2),
                  "mode": "waiting",
                  "switchpoint": 123,
        }

        self.nodeInfo.add_node(**node_1)
        self.nodeInfo.add_node(**node_2)
        
        self.backlog.add_maintenance_task(target="node_missing", hostname="ThinkPad",
                                          disks_to_reserve=None, disks_to_replace=None)

        task = SwiftMaintainAgent.incrementBacklog(nodeInfo=self.nodeInfo,
                                                   backlog=self.backlog, 
                                                   deadline=500)

        nose.tools.ok_(task["hostname"]=="ThinkPad2")
        nose.tools.ok_(task["target"]=="disk_broken")
        disks2replace = json.loads(task["disks_to_replace"])
        nose.tools.ok_(sorted(disks2replace) == ["1","2"])
        nose.tools.ok_(task["disks_to_reserve"] is None)

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))
        os.system("rm /etc/test/nodeInfo.db")
        os.system("rm /etc/test/backlog.db")

class Test_updateMaintenanceBacklog:
    '''
    Test the function SwiftEventMgr.updateMaintenanceBacklog
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

    def test_error_free(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 0,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                   ],

                         "healthy":[
                                     {"SN": "1", "timestamp": 400},
                                     {"SN": "2", "timestamp": 400},
                                     {"SN": "3", "timestamp": 400},
                                     {"SN": "4", "timestamp": 400},
                                     {"SN": "5", "timestamp": 997},
                                     {"SN": "6", "timestamp": 996},
                                   ]
        }

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 1000,
                  "disk": json.dumps(disk_info),
                  "mode": "waiting",
                  "switchpoint": 502,
        }

        node = self.nodeInfo.add_node(**node)
        

        SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=self.nodeInfo,
                                                    backlog=self.backlog, 
                                                    deadline=500)

        task = self.backlog.get_info(node["hostname"])
        nose.tools.ok_(task is None)

    def test_new_disk_missing(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 1,
                                        "timestamp": 501,
                                    },

                         "broken": [
                                   ],

                         "healthy":[
                                     {"SN": "1", "timestamp": 400},
                                     {"SN": "2", "timestamp": 400},
                                     {"SN": "3", "timestamp": 400},
                                     {"SN": "4", "timestamp": 400},
                                     {"SN": "5", "timestamp": 997},
                                     {"SN": "6", "timestamp": 996},
                                   ]
        }

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 1000,
                  "disk": json.dumps(disk_info),
                  "mode": "waiting",
                  "switchpoint": 502,
        }

        node = self.nodeInfo.add_node(**node)

        self.backlog.add_maintenance_task(target="node_missing", hostname="ThinkPad",
                                          disks_to_reserve=None, disks_to_replace=None)

        SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=self.nodeInfo,
                                                    backlog=self.backlog, 
                                                    deadline=500)


        task = self.backlog.get_info(node["hostname"])
        nose.tools.ok_(task is None)

    def test_not_in_waiting_mode(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 0,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                   ],

                         "healthy":[
                                     {"SN": "1", "timestamp": 400},
                                     {"SN": "2", "timestamp": 400},
                                     {"SN": "3", "timestamp": 400},
                                     {"SN": "4", "timestamp": 400},
                                     {"SN": "5", "timestamp": 997},
                                     {"SN": "6", "timestamp": 996},
                                   ]
        }

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 1000,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 502,
        }

        node = self.nodeInfo.add_node(**node)
        

        self.backlog.add_maintenance_task(target="node_missing", hostname="ThinkPad",
                                          disks_to_reserve=None, disks_to_replace=None)

        SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=self.nodeInfo,
                                                    backlog=self.backlog, 
                                                    deadline=500)

        task = self.backlog.get_info(node["hostname"])
        nose.tools.ok_(task is None)


    def test_node_missing_error(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 0,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                   ],

                         "healthy":[
                                     {"SN": "1", "timestamp": 400},
                                     {"SN": "2", "timestamp": 400},
                                     {"SN": "3", "timestamp": 400},
                                     {"SN": "4", "timestamp": 400},
                                     {"SN": "5", "timestamp": 997},
                                     {"SN": "6", "timestamp": 996},
                                   ]
        }

        node = {
                  "hostname": "ThinkPad",
                  "status": "dead",
                  "timestamp": 1000,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 502,
        }

        node = self.nodeInfo.add_node(**node)
        

        self.backlog.add_maintenance_task(target="disk_missing", 
                                          hostname="ThinkPad",
                                          disks_to_reserve=json.dumps("[]"), 
                                          disks_to_replace=None)

        SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=self.nodeInfo,
                                                    backlog=self.backlog, 
                                                    deadline=500)

        task = self.backlog.get_info(node["hostname"])
        nose.tools.ok_(task["target"]=="node_missing")
        nose.tools.ok_(task["disks_to_replace"] is None)
        nose.tools.ok_(task["disks_to_reserve"] is None)


    def test_disk_missing_error(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "4", "timestamp": 400},
                                     {"SN": "5", "timestamp": 600},
                                   ],

                         "healthy":[
                                     {"SN": "1", "timestamp": 400},
                                     {"SN": "2", "timestamp": 400},
                                   ]
        }

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 1000,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 502,
        }

        node = self.nodeInfo.add_node(**node)
        

        self.backlog.add_maintenance_task(target="node_missing", 
                                          hostname="ThinkPad",
                                          disks_to_reserve=None, 
                                          disks_to_replace=None)

        SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=self.nodeInfo,
                                                    backlog=self.backlog, 
                                                    deadline=500)

        task = self.backlog.get_info(node["hostname"])
        nose.tools.ok_(task["target"]=="disk_missing")
        nose.tools.ok_(sorted(json.loads(task["disks_to_reserve"])) == ["1", "2", "5"])
        nose.tools.ok_(task["disks_to_replace"] is None)

    def test_disk_broken_error(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 0,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "4", "timestamp": 400},
                                     {"SN": "5", "timestamp": 600},
                                   ],

                         "healthy":[
                                     {"SN": "1", "timestamp": 400},
                                     {"SN": "2", "timestamp": 400},
                                   ]
        }

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 1000,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 502,
        }

        node = self.nodeInfo.add_node(**node)
        

        self.backlog.add_maintenance_task(target="node_missing", 
                                          hostname="ThinkPad",
                                          disks_to_reserve=None, 
                                          disks_to_replace=None)

        SwiftMaintainAgent.updateMaintenanceBacklog(nodeInfo=self.nodeInfo,
                                                    backlog=self.backlog, 
                                                    deadline=500)

        task = self.backlog.get_info(node["hostname"])
        nose.tools.ok_(task["target"]=="disk_broken")
        nose.tools.ok_(sorted(json.loads(task["disks_to_replace"])) == ["4"])
        nose.tools.ok_(task["disks_to_reserve"] is None)

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))
        os.system("rm /etc/test/nodeInfo.db")
        os.system("rm /etc/test/backlog.db")

if __name__ == "__main__":
    pass

import nose
import sys
import os
import json
import time

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util.util import GlobalVar
from util.database import NodeInfoDatabaseBroker
from master.swiftEventMgr import SwiftEventMgr
from datetime import datetime
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))


class Test_getExpectedDeviceCnt:
    '''
    Test the function SwiftEventMgr.getExpectedDeviceCnt
    '''
    def setup(self):
        self.backup = GlobalVar.ORI_SWIFTCONF+".bak"
        os.system("cp %s %s" % (GlobalVar.ORI_SWIFTCONF, self.backup))
        os.system("cp %s/fake6.ini %s" % (WORKING_DIR, GlobalVar.ORI_SWIFTCONF))

    def test_value(self):
        diskCnt = SwiftEventMgr.getExpectedDiskCount("Dummy")
        nose.tools.ok_(diskCnt == 6, "Function getExpectedDiskCount returned wrong value")

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))


class Test_isValidDiskEvent:
    '''
    Test the fucntion SwiftEventMgr.isValidDiskEvent
    '''

    def setup(self):
        self.backup = GlobalVar.ORI_SWIFTCONF+".bak"
        os.system("cp %s %s" % (GlobalVar.ORI_SWIFTCONF, self.backup))
        os.system("cp %s/fake6.ini %s" % (WORKING_DIR, GlobalVar.ORI_SWIFTCONF))
        self.date = int(time.time())

        self.validEvent = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": "ThinkPad",
                              "data": json.dumps([
                                          { 
                                              "SN": "1",
                                              "healthy": True,
                                          },
                                          { 
                                              "SN": "2",
                                              "healthy": False,
                                          },
                                          {
                                              "SN": "3",
                                              "healthy": False,
                                          },
                                          {
                                              "SN": "4",
                                              "healthy": True,
                                          },
                                          {
                                              "SN": "5",
                                              "healthy": True,
                                          },
                              ]),
                              "time": self.date,
        }

        self.invalidEvent1 = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": "ThinkPad",
                              "data": json.dumps([
                                          { 
                                              "xxx": "1",
                                              "healthy": True,
                                          },
                              ]),
                              "time": self.date,
        }

        self.invalidEvent1 = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": "ThinkPad",
                              "data": json.dumps([
                                          { 
                                              "xxx": "1",
                                              "healthy": True,
                                          },
                              ]),
                              "time": self.date,
        }

        self.invalidEvent2 = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": "ThinkPad",
                              "data": json.dumps([
                                          { 
                                              "SN": "1",
                                          },
                              ]),
                              "time": self.date,
        }

        self.illegalHostname = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": None,
                              "data": json.dumps( [
                                          { 
                                              "SN": "1",
                                              "healthy": True,
                                          },
                              ]),
                              "time": self.date,
        }

        self.illegalTimestamp = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": None,
                              "data": json.dumps([
                                          { 
                                              "SN": "1",
                                              "healthy": True,
                                          },
                              ]),
                              "time": None ,
        }

    def test_normal_value(self):
        ret = SwiftEventMgr.isValidDiskEvent(self.validEvent)
        nose.tools.ok_(ret is True, "False alarm")

    def test_invalid_format_1(self):
        ret = SwiftEventMgr.isValidDiskEvent(self.invalidEvent1)
        nose.tools.ok_(ret is False, "Failed to detect wrong format of missing SN")

    def test_invalid_format_2(self):
        ret = SwiftEventMgr.isValidDiskEvent(self.invalidEvent2)
        nose.tools.ok_(ret is False, "Failed to detct wrong format of missing heatlthy")

    def test_illegalHostname(self):
        ret = SwiftEventMgr.isValidDiskEvent(self.illegalHostname)
        nose.tools.ok_(ret is False, "Failed to detect hostname is None")

    def test_illegalTimestamp(self):
        ret = SwiftEventMgr.isValidDiskEvent(self.illegalTimestamp)
        nose.tools.ok_(ret is False, "Failed to detect timestamp is None")

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))

class Test_updateDiskInfo:
    '''
    Test the function SwiftEventMgr.updateDiskInfo
    '''
    def setup(self):
        if os.path.exists("/etc/test/test.db"):
            os.system("rm /etc/test/test.db")
        self.backup = GlobalVar.ORI_SWIFTCONF+".bak"
        os.system("cp %s %s" % (GlobalVar.ORI_SWIFTCONF, self.backup))
        os.system("cp %s/fake6.ini %s" % (WORKING_DIR, GlobalVar.ORI_SWIFTCONF))
        self.db = NodeInfoDatabaseBroker("/etc/test/test.db")
        self.db.initialize()
        
    def test_older_event(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500}, 
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999}, 
                                   ]
        }

        node = {
                  "hostname": "ThinkPad",
                  "status": "alive",
                  "timestamp": 124,
                  "disk": json.dumps(disk_info),
                  "mode": "service",
                  "switchpoint": 123,
        }

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": "{}",
                  "time": 123,
        }

        info = self.db.add_node(**node)
        nose.tools.ok_(info)

        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret is None)

    def test_healthy_2_broken(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500}, 
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999}, 
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

        data = [
                   {"SN": "1", "healthy": False},
                   {"SN": "2", "healthy": False},
                   {"SN": "3", "healthy": False},
                   {"SN": "4", "healthy": True},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==2)
        nose.tools.ok_(ret["missing"]["timestamp"]==500)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==3)
        for disk in brokenDisks:
            nose.tools.ok_(disk["SN"] in ["1","2","3"])
            if disk["SN"] == "3":
                nose.tools.ok_(disk["timestamp"]==1001)
            else:
                nose.tools.ok_(disk["timestamp"]==500)
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==1)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] == "4")
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_new_missing(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 1,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
                                     {"SN": "5", "timestamp": 999}, 
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

        data = [
                   {"SN": "1", "healthy": False},
                   {"SN": "2", "healthy": False},
                   {"SN": "3", "healthy": False},
                   {"SN": "4", "healthy": True},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==2)
        nose.tools.ok_(ret["missing"]["timestamp"]==1001)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==3)
        for disk in brokenDisks:
            nose.tools.ok_(disk["SN"] in ["1","2","3"])
            if disk["SN"] == 3:
                nose.tools.ok_(disk["timestamp"]==1001)
            else:
                nose.tools.ok_(disk["timestamp"]==500)
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==1)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] == "4")
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_broken_2_healthy(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
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

        data = [
                   {"SN": "1", "healthy": False},
                   {"SN": "2", "healthy": True},
                   {"SN": "3", "healthy": True},
                   {"SN": "4", "healthy": True},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==2)
        nose.tools.ok_(ret["missing"]["timestamp"]==500)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==1)
        for disk in brokenDisks:
            nose.tools.ok_(disk["SN"]== "1")
            nose.tools.ok_(disk["timestamp"]==500)
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==3)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] in ["2","3","4"])
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_pure_broken_replacement(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 0,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
                                     {"SN": "5", "timestamp": 999},
                                     {"SN": "6", "timestamp": 999},
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

        data = [
                   {"SN": "7", "healthy": True},
                   {"SN": "8", "healthy": True},
                   {"SN": "3", "healthy": True},
                   {"SN": "4", "healthy": True},
                   {"SN": "5", "healthy": True},
                   {"SN": "6", "healthy": True},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==0)
        nose.tools.ok_(ret["missing"]["timestamp"]==500)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==0)
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==6)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] in ["3","4","5","6","7","8"])
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_pure_missing_replacement(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
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

        data = [
                   {"SN": "1", "healthy": False},
                   {"SN": "2", "healthy": False},
                   {"SN": "3", "healthy": True},
                   {"SN": "4", "healthy": True},
                   {"SN": "5", "healthy": True},
                   {"SN": "6", "healthy": True},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==0)
        nose.tools.ok_(ret["missing"]["timestamp"]==500)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==2)
        for disk in brokenDisks:
            nose.tools.ok_(disk["SN"] in ["1","2"])
            nose.tools.ok_(disk["timestamp"]==500)
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==4)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] in ["3","4","5","6"])
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_mixed_replacement(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
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

        data = [
                   {"SN": "1", "healthy": False},
                   {"SN": "2", "healthy": True},
                   {"SN": "3", "healthy": True},
                   {"SN": "4", "healthy": True},
                   {"SN": "5", "healthy": False},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==1)
        nose.tools.ok_(ret["missing"]["timestamp"]==500)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==2)
        for disk in brokenDisks:
            nose.tools.ok_(disk["SN"] in ["1","5"])
            if disk["SN"] == "1":
                nose.tools.ok_(disk["timestamp"]==500)
            else:
                nose.tools.ok_(disk["timestamp"]==1001)
      
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==3)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] in ["2","3","4"])
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_new_missing(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
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

        data = [
                   {"SN": "1", "healthy": False},
                   {"SN": "2", "healthy": True},
                   {"SN": "3", "healthy": True},
                   {"SN": "5", "healthy": False},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        ret = SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==2)
        nose.tools.ok_(ret["missing"]["timestamp"]==1001)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==2)
        for disk in brokenDisks:
            nose.tools.ok_(disk["SN"] in ["1","5"])
            if disk["SN"] == "1":
                nose.tools.ok_(disk["timestamp"]==500)
            else:
                nose.tools.ok_(disk["timestamp"]==1001)
      
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==2)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] in ["2","3"])
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_too_many_disks(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
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

        data = [
                   {"SN": "1", "healthy": False},
                   {"SN": "2", "healthy": True},
                   {"SN": "3", "healthy": True},
                   {"SN": "4", "healthy": False},
                   {"SN": "5", "healthy": False},
                   {"SN": "6", "healthy": True},
                   {"SN": "7", "healthy": True},
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        ret = json.loads(self.db.get_info("ThinkPad")["disk"])
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==0)
        nose.tools.ok_(ret["missing"]["timestamp"]==500)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==3)
        for disk in brokenDisks:
            nose.tools.ok_(disk["SN"] in ["1","4","5"])
            if disk["SN"] == "1":
                nose.tools.ok_(disk["timestamp"]==500)
            else:
                nose.tools.ok_(disk["timestamp"]==1001)
      
            
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==4)
        for disk in healthyDisks:
            nose.tools.ok_(disk["SN"] in ["2","3","6","7"])
            nose.tools.ok_(disk["timestamp"]==1001)

    def test_no_disks(self):
        disk_info = {
                         "timestamp": 1000,
                         "missing": {
                                        "count": 2,
                                        "timestamp": 500,
                                    },

                         "broken": [
                                     {"SN": "1", "timestamp": 500},
                                     {"SN": "2", "timestamp": 500},
                                   ],

                         "healthy":[
                                     {"SN": "3", "timestamp": 999},
                                     {"SN": "4", "timestamp": 999},
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

        data = [
        ]

        event = {
                  "component_name": "disk_info",
                  "event": "HDD",
                  "level": "ERROR",
                  "hostname": "ThinkPad",
                  "data": json.dumps(data),
                  "time": 1001,
        }

        info = self.db.add_node(**node)
        SwiftEventMgr.updateDiskInfo(event, "/etc/test/test.db")
        ret = json.loads(self.db.get_info("ThinkPad")["disk"])
        nose.tools.ok_(ret)
        nose.tools.ok_(ret["timestamp"]==1001)

        nose.tools.ok_(ret["missing"]["count"]==6)
        nose.tools.ok_(ret["missing"]["timestamp"]==1001)

        brokenDisks = [disk for disk in ret["broken"]]
        nose.tools.ok_(len(brokenDisks)==0)
        healthyDisks = [disk for disk in ret["healthy"]]
        nose.tools.ok_(len(healthyDisks)==0)

    def teardown(self):
        os.system("cp %s %s" % (self.backup, GlobalVar.ORI_SWIFTCONF))
        os.system("rm /etc/test/test.db")

if __name__ == "__main__":
    pass

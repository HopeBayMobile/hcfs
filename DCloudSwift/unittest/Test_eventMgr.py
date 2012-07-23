import nose
import sys
import os
import json
import time

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util.util import GlobalVar
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
                              "data": [
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
                              ],
                              "time": self.date,
        }

        self.invalidEvent1 = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": "ThinkPad",
                              "data": [
                                          { 
                                              "xxx": "1",
                                              "healthy": True,
                                          },
                              ],
                              "time": self.date,
        }

        self.invalidEvent1 = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": "ThinkPad",
                              "data": [
                                          { 
                                              "xxx": "1",
                                              "healthy": True,
                                          },
                              ],
                              "time": self.date,
        }

        self.invalidEvent2 = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": "ThinkPad",
                              "data": [
                                          { 
                                              "SN": "1",
                                          },
                              ],
                              "time": self.date,
        }

        self.illegalHostname = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": None,
                              "data": [
                                          { 
                                              "SN": "1",
                                              "healthy": True,
                                          },
                              ],
                              "time": self.date,
        }

        self.illegalTimestamp = {
                              "component_name": "disk_info",
                              "event": "HDD",
                              "level": "ERROR",
                              "hostname": None,
                              "data": [
                                          { 
                                              "SN": "1",
                                              "healthy": True,
                                          },
                              ],
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

if __name__ == "__main__":
    pass

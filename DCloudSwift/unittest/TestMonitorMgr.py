import nose
import sys
import os
import json
import time

# Import packages to be tested
sys.path.append('../src/DCloudSwift/')
from util.util import GlobalVar
from util import util
from master.swiftMonitorMgr import SwiftMonitorMgr
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))


class TestMonitorMgr:
    def setup(self):
        self.storageList = [
                        {"hostname": "TPE01", "ip": "192.168.11.1", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE02", "ip": "192.168.11.2", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE03", "ip": "192.168.11.3", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE04", "ip": "192.168.11.4", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE05", "ip": "192.168.11.5", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE06", "ip": "192.168.11.6", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE07", "ip": "192.168.11.7", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE08", "ip": "192.168.11.8", "deviceCnt": 6, "deviceCapacity": 100}, 
                        {"hostname": "TPE09", "ip": "192.168.11.9", "deviceCnt": 6, "deviceCapacity": 100}, 
                      ]
        self.num_of_replica = 3
        self.user_usage = {
            'CTBD': {'Ben': {'usage': 10},
                     'Cally': {'usage': 10},
                     },
            'IT': {'Roger': {'usage': 10},
                   'DMC': {'usage': 110},
                  },
            'IABU': {'Robert': {'usage': 100}}
        }

    def testGetNumberOfStorageNodes(self):
        SM = SwiftMonitorMgr()
        num_of_nodes = SM.get_number_of_storage_nodes(self.storageList)    
        nose.tools.ok_(num_of_nodes == 9)

    def testGetTotalCapacity(self):
        SM = SwiftMonitorMgr()
        total_capacity = SM.get_total_capacity(self.storageList, self.num_of_replica)    
        nose.tools.ok_(int(total_capacity) == 1800)

    def testGetUsedCapacity(self):
        SM = SwiftMonitorMgr()
        used_capacity = SM.get_used_capacity(self.user_usage)    
        print used_capacity
        nose.tools.ok_(used_capacity == 240)

    def teardown(self):
        pass

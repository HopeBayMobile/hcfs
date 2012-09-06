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
        nose.tools.ok_(used_capacity == 240)

    def testGetHdInfo(self):
        disk_info = {"missing": { "count": 2, "timestamp": 123},
                     "healthy": [{"SN": "a"}, {"SN": "b"}],
                     "broken": [{"SN": "c"}, {"SN": "d"}]
                    }

        disk_info_json = json.dumps(disk_info)
        SM = SwiftMonitorMgr()
        hd_info = SM.get_hd_info(disk_info_json)

        nose.tools.ok_(len(hd_info) == 6)
        for disk in hd_info:
            nose.tools.ok_(disk["capacity"] == "N/A")
            if disk["serial"] == "a" or disk["serial"] == "b":
                nose.tools.ok_(disk["status"] == "OK")
            elif disk["serial"] == "c" or disk["serial"] == "d":
                nose.tools.ok_(disk["status"] == "Broken")
            else:
                nose.tools.ok_(disk["status"] == "Missing") 

    def testCalculateTotalCapacityInTB(self):
        total = 0
        SM = SwiftMonitorMgr()
        total_TB = SM.calculate_total_capacity_in_TB(total)
        nose.tools.ok_(total == 0)

        total = 1000
        total_TB = SM.calculate_total_capacity_in_TB(total)
        nose.tools.ok_(isinstance(total_TB, float) == True)
        nose.tools.ok_(total_TB == 1/float(1000*1000*1000))


        total = 1000*1000*1000*1000*100
        total_TB = SM.calculate_total_capacity_in_TB(total)
        nose.tools.ok_(isinstance(total_TB, float) == True)
        nose.tools.ok_(total_TB == 100)


    def testCalculateUsedCapacityPercentage(self):
        total = 0
        used = 2
        SM = SwiftMonitorMgr()
        percentage = SM.calculate_used_capacity_percentage(total, used)
        nose.tools.ok_(percentage is None)

        total = 200
        used = 0
        SM = SwiftMonitorMgr()
        percentage = SM.calculate_used_capacity_percentage(total, used)
        nose.tools.ok_(isinstance(percentage, float))
        nose.tools.ok_(percentage == 0)

        total = 200
        used = 100 
        SM = SwiftMonitorMgr()
        percentage = SM.calculate_used_capacity_percentage(total, used)
        nose.tools.ok_(isinstance(percentage, float))
        nose.tools.ok_(percentage == 50)

    def testCalculateFreeCapacityPercentage(self):
        total = 0
        used = 2
        SM = SwiftMonitorMgr()
        percentage = SM.calculate_free_capacity_percentage(total, used)
        nose.tools.ok_(percentage is None)

        total = 200
        used = 0
        SM = SwiftMonitorMgr()
        percentage = SM.calculate_free_capacity_percentage(total, used)
        nose.tools.ok_(isinstance(percentage, float))
        nose.tools.ok_(percentage == 100)

        total = 200
        used = 100
        SM = SwiftMonitorMgr()
        percentage = SM.calculate_free_capacity_percentage(total, used)
        nose.tools.ok_(isinstance(percentage, float))
        nose.tools.ok_(percentage == 50)

        total = 200
        used = 100
        unusable = 100
        SM = SwiftMonitorMgr()
        percentage = SM.calculate_free_capacity_percentage(total, used, unusable)
        nose.tools.ok_(isinstance(percentage, float))
        nose.tools.ok_(percentage == 0)

    def testGetHdError(self):
        disk_info = {"missing": { "count": 2, "timestamp": 123},
                     "healthy": [{"SN": "a"}, {"SN": "b"}],
                     "broken": [{"SN": "c"}, {"SN": "d"}]
                    }

        disk_info_json = json.dumps(disk_info)
        SM = SwiftMonitorMgr()
        errors = SM.get_hd_error(disk_info_json)
        nose.tools.ok_(errors == 4)


    def teardown(self):
        pass

import os
import sys
import time
import socket
import random
import pickle
import signal
import json
import sqlite3
from ConfigParser import ConfigParser

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.insert(0,"%s/DCloudSwift/" % BASEDIR)

from util.SwiftCfg import SwiftMasterCfg
from util.util import GlobalVar
from util import util
from util.database import NodeInfoDatabaseBroker
from util.database import MaintenanceBacklogDatabaseBroker


class SwiftMonitorMgr:
    """
    Mockup for SwiftMonitorMgr
    """
    
    def __init__(self):
        self.logger = util.getLogger(name="SwiftMonitorMgr")

    def get_total_capacity(self):
        '''
        return the total capacity of the capacity or none if errors happen
        '''
        storageList = util.getStorageNodeList()
        capacity = 0
        try:
            if storageList:
                for node in storageList:
                    capacity += node["deviceCapacity"] * node["deviceCnt"]
            else:
                return None
        except Exception as e:
             self.logger.error(str(e))
             return None

        return capacity

    def get_number_of_storage_nodes(self):
        '''
        return the number of the nodes or none if errors happen.
        '''

        storageList = util.getStorageNodeList()
        try:
            # number of storage nodes must be greater than zero
            if storageList:
                return len(storageList)
            else:
                return None
        except Exception as e:
             self.logger.error(str(e))
             return None

    def get_used_capacity(self):
        '''
        return used capacity in bytes
        '''
        # TODO: fill in contents
        return 4300000000

    def calculate_total_capacity_in_TB(self, total):
        '''
        calculate used_capacity_percentage
        @param total: total capacity in bytes
        return total capacity in TB
        '''
        total_TB = None
        try:
            total_TB = float(total)/float(1024*1024*1024*1024)
        except Exception as e:
            self.logger.error(str(e))

        return total_TB


    def calculate_used_capacity_percentage(self, total, used):
        '''
        calculate used_capacity_percentage
        @param total: total capacity
        @param used: used capacity
        return used capacity percentage
        '''
        percentage = None
        try:
            percentage = (float(used)/float(total)) * 100
        except Exception as e:
             self.logger.error(str(e))

        return percentage


    def calculate_free_capacity_percentage(self, total, used, unusable=0):
        '''
        calculate free capacity
        @param total: total capacity
        @param used: used capacity
        @param unusable: unusable capacity due to broken nodes or disks 
        '''
        
        percentage = None
        try:
            free = float(total) - float(used) - float(unusable)
            percentage = (free/float(total))*100
        except Exception as e:
            self.logger.error(str(e))
        
        return percentage

    def get_portal_url(self):
        return util.getPortalUrl()

    def get_zone_info(self):
        """
        Get zone related infomations
        
        Essential columns:
        ip: node management console ip
        nodes: total number of node
        used: zone used storage percentage
        free: zone free storage percentage
        capacity: total zone capacity
        """

        url = self.get_portal_url()
        nodes = self.get_number_of_storage_nodes()
        total_capacity = self.get_total_capacity()
        used_capacity = self.get_used_capacity()

        capacity = free = used = "N/A"

        if total_capacity:
            capacity = "%.0fTB" % (self.calculate_total_capacity_in_TB(total_capacity))
            if used_capacity:
                used = "%.2f" % (self.calculate_used_capacity_percentage(total_capacity, used_capacity))
                free = "%.2f" % (self.calculate_free_capacity_percentage(total_capacity, used_capacity))
            
        zone = {"url": url, "nodes": nodes, "used": used, "free": free, "capacity": capacity}
        
        return zone

    def list_nodes_info(self):
        """
        Get all nodes infomations
        
        Essential columns:
        ip: node private ip
        index: enumerate number of the node
        hostname: node alias name
        status: node current stat (dead or alive)
        mode: node operation mode (waiting or service)
        hd_number: node's total hard disk number
        hd_error: node's total hard disk error number
        hd_ino: dictionary of each hard disk status in this node, which include:
            serial: serial number
            status: operation status (OK or Broken)
        
        """
        nodes_info = []
        nodes_info.append({"ip":"172.30.11.33","index":"1","hostname":"TPEIIA","status":"dead","mode":"waiting","hd_number":6,"hd_error":1,
        "hd_info":[{"serial":"SN_TP02","status":"Broken"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
        })
        nodes_info.append({"ip":"172.30.11.37","index":"2","hostname":"TPEIIB","status":"alive","mode":"waiting","hd_number":6,"hd_error":0,
        "hd_info":[{"serial":"SN_TP02","status":"OK"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
        })
        nodes_info.append({"ip":"172.30.11.25","index":"3","hostname":"TPEIIC","status":"alive","mode":"service","hd_number":6,"hd_error":0,
        "hd_info":[{"serial":"SN_TP02","status":"OK"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
        })
        return nodes_info

if __name__ == '__main__':
    SM = SwiftMonitorMgr()
    print SM.get_zone_info()

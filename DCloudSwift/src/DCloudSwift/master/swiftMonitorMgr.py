import os
import sys
import time
import socket
import random
import pickle
import signal
import json
import sqlite3
import pkg_resources
from ConfigParser import ConfigParser

WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.insert(0, "%s/DCloudSwift/" % BASEDIR)

from util.SwiftCfg import SwiftMasterCfg
from util.util import GlobalVar
from util import util
from util.database import NodeInfoDatabaseBroker
from util.database import MaintenanceBacklogDatabaseBroker

from swiftAccountMgr import SwiftAccountMgr

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
        num_of_replica = util.getNumOfReplica()

        if not num_of_replica:
            return None

        try:
            if storageList:
                for node in storageList:
                    capacity += node["deviceCapacity"] * node["deviceCnt"]
            else:
                return None

            capacity = capacity/float(num_of_replica)

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
        SA = SwiftAccountMgr()
        result = SA.list_usage()
        if not result.val:
            return None

        total_usage = 0
        for account, users in result.msg:
            for user, usage in users:
                total_usage += usage

        return total_usage

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

    def get_hd_error(self, disk_info_json):
        '''
        calculate ddfree capacity
        @param total: total capacity
        @param used: used capacity
        @param unusable: unusable capacity due to broken nodes or disks 
        '''
        disk_info = json.loads(disk_info_json)

        error = 0
        missing = disk_info.get("missing", {})
        error += missing.get("count", 0)

        for hd in disk_info.get("broken", []):
            error += 1
        
        return error

    def get_hd_info(self, disk_info_json):
        '''
        calculate ddfree capacity
        @param total: total capacity
        @param used: used capacity
        @param unusable: unusable capacity due to broken nodes or disks 
        '''
        disk_info = json.loads(disk_info_json)

        hd_info = []
        for hd in disk_info.get("healthy", []):
            hd_info.append({"serial": hd.get("SN", "N/A"), "status": "OK"})

        for hd in disk_info.get("broken", []):
            hd_info.append({"serial": hd.get("SN", "N/A"), "status": "Broken"})
        
        return hd_info

    def get_portal_url(self):
        return util.getPortalUrl()

    def get_private_ip(self, hostname, nameserver="192.168.11.1"):
        '''
        lookup ip addresses of hosts.

        @param  hostname: hostname to lookup ip
        @param nameserver: ip address of the nameserver, and the default
            value is 192.168.11.1
        @return: private ip of hostname
        '''

        ip = util.hostname2Ip(hostname=hostname, nameserver=nameserver)  # This function returns None if lookup failed

        return ip

    def get_swift_version(self):
        '''
        return swift version
        '''
        version = ""
        try:
            version = pkg_resources.require("swift")[0].version
        except Exception as e:
            self.logger.error(str(e))
       
        return version
 
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
        firmware = "swift_" + self.get_swift_version()

        capacity = free = used = "N/A"

        if total_capacity:
            capacity = "%.0fTB" % (self.calculate_total_capacity_in_TB(total_capacity))
            if used_capacity:
                used = "%.2f" % (self.calculate_used_capacity_percentage(total_capacity, used_capacity))
                free = "%.2f" % (self.calculate_free_capacity_percentage(total_capacity, used_capacity))
            
        zone = {"ip": url, "nodes": nodes, "used": used, "free": free, "capacity": capacity, "firmware": firmware}
        
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
        db = NodeInfoDatabaseBroker(util.GlobalVar.NODE_DB)
        node_info_table = db.show_node_info_table()

        if not node_info_table:
            return nodes_info

        index = 0
        for info in node_info_table:
            index += 1
            disk_info_json = info["disk"]
            spec = db.get_spec(info["hostname"])
            if not spec:
                self.logger.error("Failed to get spec of %s" % info["hostname"])
            
            hostname = info["hostname"]
            ip = self.get_private_ip(hostname=hostname)
            status = info["status"]
            mode = info["mode"]
            hd_number = spec["diskcount"] if spec else 0
            hd_error = self.get_hd_error(disk_info_json)
            hd_info = self.get_hd_info(disk_info_json)

            nodes_info.append({"hostname": hostname,
                               "index": str(index), 
                               "ip": ip,
                               "status": status, 
                               "mode": mode, 
                               "hd_number": hd_number,
                               "hd_error": hd_error,
                               "hd_info": hd_info,
                              })

        return nodes_info

if __name__ == '__main__':
    SM = SwiftMonitorMgr()
    print SM.get_zone_info()
    #print SM.list_nodes_info()

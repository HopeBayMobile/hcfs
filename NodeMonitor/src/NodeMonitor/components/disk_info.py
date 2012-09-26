#!/usr/bin/env python
import sys
import subprocess
import os
import json
import urllib
import re

MODULE_NAME = 'DISK_INFO'


class DiskInfo:

    def __init__(self):
        self.device_prx = '/srv/node/sdb'

    def get_mounted_disks(self):
        '''
        return the set of all mounted disks
        '''
        devices = set()
        cmd = "sudo mount"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()

        for line in lines:
            data = line.strip().split()
            block_device = data[0]
            match = re.match(r"^/dev/sd\w", block_device)
            if match:
                devices.add(block_device[:8])
        return devices

    def get_all_disks(self):
        cmd = "sudo smartctl --scan"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()

        disks = []
        for line in lines:
                match = re.match(r"^/dev/sd\w", line)
                if match is not None:
                        disks.append(line.split()[0][:8])

        return disks

    def is_healthy(self, disk):
        """
        check if the given disk is healthy
        @type  disk: string
        @param disk: name of the disk to check health
	@rtype: bool 
        @return: True iff the disk is healthy and mounted
        """

        mounted_disks = self.get_mounted_disks()
        if not disk in mounted_disks:
            return False

        cmd ="sudo smartctl -H %s" % disk
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        output = output.lower()
        if output.find("smart overall-health self-assessment test result: passed") != -1:
            return True
        else:
            return False

    def get_capacity(self, disk):
        """
        get the capacity of the disk
        @type  disk: string
        @param disk: name of the disk to check capacity
	@rtype: int 
        @return: capacity of the disk in bytes.
        """

        cmd = "sudo smartctl -i %s" % disk
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        total_bytes = None
        for line in lines:
            line = line.lower()
       	    if line.find("capacity:") == -1:
                continue
            
            value = line.partition(":")[2]
            total_bytes_in_str = value.split()[0]
            total_bytes = int(total_bytes_in_str.replace(",",""))

        return total_bytes

    def get_used_space(self, disk):
        """
        get the used space of the disk
        @type  disk: string
        @param disk: name of the disk to check used space
	@rtype: int 
        @return: used space of the disk in bytes.
        """

        cmd = "sudo df -B1 -a"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        total_bytes = 0
        for line in lines:
            line = line.lower()
            if line.startswith(disk):
                total_bytes += int(line.split()[2])

        return total_bytes

    def get_serial_number(self, disk):
        """
        get the serial number of the disk
        @type  disk: string
        @param disk: name of the disk to check health
	@rtype: str
        @return: serial number of the disk
        """

        cmd = "sudo smartctl -i %s" % disk
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        serial_number = None
        for line in lines:
            line = line.lower()
       	    if line.find("serial number:") == -1:
                continue
            
            value = line.partition(":")[2]
            serial_number = value.split()[0]
            break

        return serial_number

    def check_disk(self, disk):
        """
        check status of the given disk

	@rtype:
                    {
                       "SN": "serial number of the disk", 
                       "healthy": bool, 
                       "capacity": capacity of the disk in GB, 
                       "usage": used space of the disk in GB
                    }
  
        @return: info of the disk
        """

        disk_info = {}
        disk_info["SN"] = self.get_serial_number(disk)
        disk_info["healthy"] = self.is_healthy(disk)
        disk_info["capacity"] = self.get_capacity(disk)
        if disk_info["capacity"] == 0:
            disk_info["usage"] = 100
        else:
            used_bytes = self.get_used_space(disk)
            
            try:
                disk_info["usage"] =  int((float(used_bytes) / float(disk_info["capacity"])) * 100)
            except TypeError:
                disk_info["usage"] = None
 
        return disk_info

    def check_all_disks(self):
        """
        check status of all detected disks

	@rtype: [
                    {
                       "SN": "serial number of the disk", 
                       "healthy": bool, 
                       "capacity": capacity of the disk in bytes, 
                       "usage": used percentage of the disk
                    }, ...
        ]
  
        @return: info of all detected disks
        """
        disks = self.get_all_disks()
        ret = []
        for disk in disks:
            ret.append(self.check_disk(disk))

        return ret
        

def get_module_name():
    '''
    '''
    return MODULE_NAME
    
def module_init():
    '''
    module_init()
    '''
    print "init"
    
def module_read():
    '''
    module_read()
    '''
    DI = DiskInfo()
    data = DI.check_all_disks()
    return json.dumps(data)   
    
def module_config(sKey, sValue):
    '''
    module_config()
    '''
    if sKey == "dev":
        __DEV__ = sValue
    
def module_option(sOption):
    '''
    module_option
    '''

def module_log(log_file):
    '''
    module_log
    assign the component logging file.
    '''

if __name__ == '__main__':
    '''
    main()
    '''

    for i in range(len(sys.argv)):    
        if sys.argv[i] == "--init":
            module_init()
            i += 1
        elif sys.argv[i] == "--read":
            print module_read()
            i += 1
        elif sys.argv[i] == "--option":
            module_option(sys.argv[2])
            i += 2
        elif sys.argv[i] == "--config":
            module_option(sys.argv[2], sys.argv[3])
            i += 3
        elif sys.argv[i] == "--get-name":
            print get_module_name() 
            i += 1

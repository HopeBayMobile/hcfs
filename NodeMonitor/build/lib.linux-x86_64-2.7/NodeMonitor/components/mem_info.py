#!/usr/bin/env python
import sys
import subprocess
import os
import json
import urllib
import re

MODULE_NAME = 'MEM_INFO'

class MemInfo:

    def __init__(self):
        pass

    def get_total(self):
        """
        get total number of memory
	@rtype: int
        @return: total number of memory in bytes
        """

        cmd ="sudo cat /proc/meminfo"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        mem = None
        for line in lines:
            line = line.lower()
            if line.find("memtotal:") == -1:
                continue
            value = line.partition(":")[2]
            mem = int(value.split()[0]) * 1024

        return mem

    def get_usage(self):
        """
        get used memory in percentage
	@rtype: int 
        @return: used memory in percentage
        """

        cmd ="sudo cat /proc/meminfo"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        total = self.get_total()
        free = None
        used_percentage = None

        for line in lines:
            line = line.lower()
            if line.find("memfree:") == -1:
                continue
            value = line.partition(":")[2]
            free = int(value.split()[0]) * 1024

        if total and free:
            used = total - free
            used_percentage = int((float(used) / float(total)) * 100)

        return used_percentage

    def check_memory(self):
        """
        check memory

	@rtype:
                    {
                       "total": total number of memory in bytes 
                       "usage": percentage of used memory
                    }
  
        @return: info of memory
        """

        mem_info = {}
        mem_info["total"] = self.get_total()
        mem_info["usage"] = self.get_usage()
        return mem_info

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
    MI = MemInfo()
    data = MI.check_memory()
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

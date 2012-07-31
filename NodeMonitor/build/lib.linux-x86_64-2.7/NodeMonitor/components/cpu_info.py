#!/usr/bin/env python
import sys
import subprocess
import os
import json
import urllib
import re

MODULE_NAME = 'CPU_INFO'

class CpuInfo:

    def __init__(self):
        pass

    def get_total(self):
        """
        get total number of CPUs

	@rtype: int 
        @return: total number of CPUs
        """

        cmd ="sudo mpstat"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        match = re.search(r"\(([0-9]+) CPU\)", output)  # match expression like (2 CPU)
        if match:
            return int(match.group(1))

        return match


    def get_usage(self):
        """
        check cpu

	@rtype:[
                    {
                       "id": cpu id (integer)
                       "usage": cpu usage in percentage (integer)
                    }, ...
        ]
  
        @return: usage of all CPUs 
        """

        cmd = "sudo mpstat -P ALL"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        if po.returncode !=0 or len(lines) < 4:
            return []

        #skip the first 4 lines
        lines = lines[4:]

        cpu_info = []
        cpu_id = 0
        try:
            for line in lines:
                tokens = line.split()
                if len(tokens) < 12:
                    return []

                if cpu_id != int(tokens[2]):
                    return []

                usage = int(100.0 - float(tokens[11]))
                cpu_info.append({"id": cpu_id, "usage": usage})
                cpu_id += 1

            return cpu_info
        except Exception:
            return []

    def check_cpu(self):
        """
        check cpu

	@rtype:[
                    {
                       "id": cpu id (integer)
                       "usage": cpu usage in percentage (integer)
                    }, ...
        ]
  
        @return: info of CPUs
        """

        cpu_usage = self.get_usage()
        return cpu_usage

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
    CI = CpuInfo()
    data = CI.check_cpu()
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

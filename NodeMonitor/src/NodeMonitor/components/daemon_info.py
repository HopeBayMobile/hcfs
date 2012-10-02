#!/usr/bin/env python
import sys
import subprocess
import os
import json
import urllib
import re

MODULE_NAME = 'DAEMON_INFO'

DAEMONS = ["account-server", "account-replicator", 
           "container-server", "container-replicator", "container-updater",
           "object-server", "object-replicator", "object-updater",
           "proxy-server",
           "memcached",
           "rsync"]


class DaemonInfo:

    def __init__(self):
        pass

    def check_daemons(self):
        """
        check statuses of daemons
	@rtype: dictionary
        @return: {"daemon_name1": "on", "daemon_name2": "off", ...}
        """

        result = {}
        for daemon in DAEMONS:
            cmd ="sudo ps -ef | grep %s | grep -v grep" % daemon
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            lines = po.stdout.readlines()
            po.wait()

            if po.returncode != 0 or not lines:
                result[daemon] = "off"
            else:
                result[daemon] = "on"

        return result

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
    DI = DaemonInfo()
    data = DI.check_daemons()
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

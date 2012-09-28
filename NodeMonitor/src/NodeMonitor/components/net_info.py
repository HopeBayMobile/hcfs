#!/usr/bin/env python
import sys
import subprocess
import os
import json
import urllib
import time
import re

MODULE_NAME = 'NET_INFO'

class NetInfo:

    def __init__(self):
        pass

    def get_usages(self):
        """
        get ethernet interfaces
	@rtype: 
                  {
                       "eth0": { "receive": bytes received by the interface (integer),
                                 "transmit" bytes transmitted by the interface (interger)},
                       "eth1": { ... },
                       ...
                  }

        @return: usages of ethernet interfaces
        """

        ret = {}
        cmd ="sudo cat /proc/net/dev"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        if po.returncode != 0:
            return ret

        for line in lines:
            line = line.lower()
            line = line.strip()
            if not line.startswith("eth") and not line.startswith("wlan"):
                continue
            name, _, value = line.partition(":")
            receive = None
            transmit = None
            try:
                tokens = value.split()
                receive = int(tokens[0])
                transmit = int(tokens[8])
            except:
                #TODO: error handling
                pass
                
            ret[name] = { "receive": receive, "transmit": transmit}

        return ret

    def get_usage(self, interface):
        """
        get the usage of the ethernet interface
        @type interface: string
        @param interface: name of the interface
	@rtype: 
                  {
                   "interface": name of the interface
                   "receive": bytes received by the interface (integer),
                   "transmit" bytes transmitted by the interface (interger),
                  }
        @return: usage of the ethernet interface
        """

        ret = {}
        receive = None
        transmit = None

        cmd ="sudo cat /proc/net/dev"
        po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        lines = po.stdout.readlines()
        po.wait()

        if po.returncode != 0:
            return ret

        for line in lines:
            line = line.lower().strip()
            if not line.startswith(interface):
                continue

            name, _, value = line.partition(":")
            try:
                tokens = value.split()
                receive = int(tokens[0])
                transmit = int(tokens[8])
            except:
                #TODO: error handling
                pass

            break

        ret["interface"] = interface
        ret["receive"] = receive
        ret["transmit"] = transmit
        return ret

    def get_transfer_rates(self, interval=1):
        """
        get transfer rates of ethernet interfaces during an interval starting from now
        @type interval: integer
        @param interval: time interval to measure the transfer rates (in seconds)
	@rtype: {
                   "eth0": {"receive_rate": bits received by the interface per second (int),
                            "transmit_rate" bits transmitted by the interface per second (int)},
                    ...
                }
        @return: transfer rates of ethernet interfaces
        """

        interval = 1 if interval < 1 else interval
        usages = self.get_usages()
        time.sleep(interval)
        ret = {}

	for interface, start in usages.items():
            end = self.get_usage(interface)
            receive_rate = None
            transmit_rate = None

            if not end["receive"] is None and not start["receive"] is None:
                receive_rate = (end["receive"] - start["receive"]) * 8 / interval
            if not end["transmit"] is None and not start["transmit"] is None:
                transmit_rate = (end["transmit"] - start["transmit"]) * 8 / interval

            ret[interface] = { "receive_rate": receive_rate,
                               "transmit_rate": transmit_rate,
                             }

        return ret

    def check_network(self, interval=1):
        """
        check network status
        @type interval: integer
        @param interval: time interval to measure the transfer rates (in seconds)
	@rtype: [
                  {
                   "interface": name of the ethernet interface,
                   "receive_rate": bits received by the interface per second (int),
                   "transmit_rate" bits transmitted by the interface per second (int),
                  }, ...
                ]
        @return: info of ethernet interfaces
        """
   
        ret = self.get_transfer_rates(interval=interval)
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
    NI = NetInfo()
    data = NI.check_network()
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

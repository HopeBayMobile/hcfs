#!/usr/bin/env python
#-*- coding: utf-8 -*-
'''
Created on 2011/6/30

@author: cloudusr
'''
import netifaces
import os
import random

__DEBUG__ = True

class NetworkEnvironment(object):
    '''
    classdocs
    '''
    ifaces = []
    ifconfig = {}

    def __init__(self):
        '''
        Constructor
        '''
        self.get_interfaces()        

    def get_interfaces(self):
        #self.__class__.ifaces = netifaces.interfaces()    #netifaces.interfaces()得知host端有幾個NIC
	#print netifaces.interfaces()
        for iface in netifaces.interfaces():
	    if ('eth' in iface or 'br' in iface):
		#for iface in ["eth1", "br100"]:
	        #print iface
	        self.__class__.ifconfig[iface] = {}
	        self.__class__.ifconfig[iface]['switch_mac'] = '00:00:00:00:00:00'
	        self.__class__.ifconfig[iface]['ipv4'] = self.get_ipv4_address(iface)
	        self.__class__.ifconfig[iface]['ipv6'] = self.get_ipv6_address(iface)
	        hwaddr = self.get_mac_address(iface)
	        self.__class__.ifconfig[iface]['nic_mac'] = hwaddr
	        if(iface != 'lo'):
	   	    self.__class__.ifconfig[iface]['switch_port_number'] = self.get_port_number(hwaddr)
	    else: 
		#print iface
		pass
		
    def get_ipv4_address(self, ifname):                            
        iface = netifaces.ifaddresses(ifname) 
        if ifname in netifaces.interfaces():
            try:
                ifaceaddr = iface[netifaces.AF_INET]
                ipv4_address = ifaceaddr[0]['addr']
            except:
                ipv4_address = ''

        return ipv4_address
    
    def get_ipv6_address(self, ifname):
        ipv6_address = ''
        if ifname in netifaces.interfaces():
            iface = netifaces.ifaddresses(ifname)
            try:
                ifaceaddr = iface[netifaces.AF_INET6]
                ipv6_address = ifaceaddr[0]['addr'].replace('%' + ifname, '') # replace %eth0 to space
            except:
                ipv6_address = ''

        return ipv6_address

    def get_mac_address(self, ifname):
        mac_address = ''
        if ifname in netifaces.interfaces():
            iface = netifaces.ifaddresses(ifname)
            try:
                ifaceaddr = iface[netifaces.AF_LINK]
                mac_address = ifaceaddr[0]['addr']
            except:
                mac_address = ''

        return mac_address;
    # query port number form switch by snmp
    def get_port_number(self, hwaddr):
        port = 0
        if(__DEBUG__):
            port = random.randrange(1, 1000, 2)
        else:
            results = os.popen("libs/scanPort.sh " + hwaddr)
            data = []
            for line in results.readlines():
                line = line.strip()
                data = line.split(' ')
            try:
                port = data[3]
            except:
                port = -1

        return port
    def get_hostname(self):
        '''hostname = '''
        hostname = os.popen("cat /etc/hostname")
        hostname = hostname.read().strip()
        '''for line in results.readlines():
            line = line.strip()
            hostname = line'''
        return hostname
    def get_baseimage(self):
        '''baseimage = '''
        base_image = os.popen("cat /etc/issue")
        baseimage = base_image.read().strip()
        return baseimage
        ''''a = "Ubuntu 11.04"
        baseimage = a
        return baseimage'''

#!/usr/bin/env python
#-*- coding: utf-8 -*-
'''
Created on Aug 12, 2011

@author: juitsung
'''
import os
import re
import pexpect

SNMPVERSION=1
SNMPCOMMUNITY="public"
SNMPWALK="/usr/bin/snmpwalk"
__DEBUG__ = False

class SwitchInfo(object):
    '''
    classdocs
    '''
    clear_switch = False
    switch_address = ''

    def __init__(self, ipv4, clear_switch=False):
        '''
        Constructor
        '''
        self.__class__.switch_address = ipv4
        self.__class__.clear_switch = clear_switch
        if(self.__class__.clear_switch):
            self.clear_switch_cache()
        
    def parse(self, data):
        value = ''
        type = ''
        try:
            match = re.match("(.*)[\s\t]=[\s\t](.*):[\s\t](.*)$", data)
            type = match.group(2)
            value = match.group(3)
        except:
            try:
                match = re.match("(.*)[\s\t]=[\s\t](.*)$", data)
                value = match.group(2)
            except:
                value = ''
        return value

    def get_system_name(self):
        system_name = ''
        FDBTABLE = "1.3.6.1.2.1.1.5"
        ipv4 = self.__class__.switch_address
        cmd = ("%s -v%d -c%s %s %s") % (SNMPWALK, SNMPVERSION, SNMPCOMMUNITY, ipv4, FDBTABLE)
        for line in os.popen(cmd):
            line = line.strip().replace('"', '')
            system_name = self.parse(line)
        return system_name

    def get_hardware_address(self):
        ipv4 = self.__class__.switch_address
        hardware_address = ''
        FDBTABLE = "1.3.6.1.2.1.2.2.1.6.417"
        cmd = ("%s -v%d -c%s %s %s") % (SNMPWALK, SNMPVERSION, SNMPCOMMUNITY, ipv4, FDBTABLE)
        for line in os.popen(cmd):            
            line = line.strip()
            hardware_address = self.parse(line)
        hardware_address = hardware_address.replace(' ', ':')
        return hardware_address

    def scan_port_active(self):
        ports = {}
        index = 0
        #results = os.popen("libs/clearSwitchCache.exp " + ipv4)
        ipv4 = self.__class__.switch_address
        FDBTABLE = "1.3.6.1.2.1.17.7.1.2.2.1.2"
        cmd = ("%s -v%d -c%s %s %s") % (SNMPWALK, SNMPVERSION, SNMPCOMMUNITY, ipv4, FDBTABLE)
        for line in os.popen(cmd):
            line = line.strip()
            results = re.split('\D+', line)
            port_number = int(results[20])
            if(port_number <= 48 and port_number > 0):
                mac1 = int(results[14])
                mac2 = int(results[15])
                mac3 = int(results[16])
                mac4 = int(results[17])
                mac5 = int(results[18])
                mac6 = int(results[19])
                mac_address = "%02X:%02X:%02X:%02X:%02X:%02X" % (mac1, mac2, mac3, mac4, mac5, mac6)
                ports[port_number] = mac_address
                index = index + 1
        if __DEBUG__:
            print "+------------+---------------------+"
            print "|    Port    |     MAC address     |"
            print "+------------+---------------------+"
            for item in ports.items():
                print "|     %02d     |  %17s  |" % (int(item[0]), item[1])
            print "+------------+---------------------+"
        
        return ports

    def fetch_port_number(self, mac_address):
        port_number = -1
        active_ports = self.scan_port_active()
        
        mac_address = mac_address.upper()
        for item in active_ports.items():
            if mac_address == item[1]:
                port_number = item[0]
                break
        return port_number

    def clear_switch_cache(self, user = 'admin', password = '', timeout = 10):
        ipv4 = self.__class__.switch_address
        result = False
        cmd = "telnet %s" % ipv4
        try:
            con = pexpect.spawn(cmd)
            index = con.expect(['User:', pexpect.TIMEOUT, pexpect.EOF], timeout)
            if index == 0:
                con.sendline(user)
                index = con.expect(['Password:'], timeout)
                if index == 0:
                    con.sendline(password)                    
                    con.expect(['>'], timeout)
                    con.sendline('en')
                    con.expect('#', timeout)
                    con.sendline('clear arp-cache')
                    con.expect('#', timeout)
                    con.sendline('clear mac-addr-table all')
                    con.expect('#', timeout)
                    con.sendline('exit')
            result = True
        except:
            print 'clear switch cache timeout ! %s' % ipv4
            result = False
        return result